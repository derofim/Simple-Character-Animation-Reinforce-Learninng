﻿#include "012_enviroment.h"
#include "../CommonEnviroment.h"

#include "btBulletDynamicsCommon.h"

#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"

#include "CommonInterfaces/CommonRigidBodyBase.h"
#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif


#include <cstdlib>
#include "ArmRL.h"


struct ArmNNTest : public CommonRigidBodyBase
{
	bool m_once;
	btScalar distance_;
	btAlignedObjectArray<btJointFeedback*> m_jointFeedback;
	btHingeConstraint* hinge_shader;
	btHingeConstraint* hinge_elbow;
	btRigidBody* linkBody;
	btVector3 groundOrigin_target;
	btRigidBody* body;
	btRigidBody* human_body;

	ArmNNTest(struct GUIHelperInterface* helper);
	virtual ~ArmNNTest();
	virtual void initPhysics();

	virtual void stepSimulation(float deltaTime);
	void lockLiftHinge(btHingeConstraint* hinge);

	virtual bool keyboardCallback(int key, int state);
	virtual void resetCamera()
	{
		float dist = 5;
		float pitch = 270;
		float yaw = 21;
		float targetPos[3] = { -1.34,3.4,-0.44 };
		m_guiHelper->resetCamera(dist, pitch, yaw, targetPos[0], targetPos[1], targetPos[2]);
	}

	/********************************************************************************************
	* start custom Functions
	*********************************************************************************************/

	ArmRL *rl_;
	ArmRL *rl_sd_;

	VectorND<float> state_buffer_;

	bool chkModeStudying = true;

	void initState(ArmNNTest* target) {
		target->m_guiHelper->removeAllGraphicsInstances();
		target->initPhysics();
	}

	void moveIn(btHingeConstraint *target) {
		target->setLimit(M_PI / 360.0f, M_PI / 1.2f);
		target->enableAngularMotor(true, 6.0, 4000.f);

	}

	void moveOut(btHingeConstraint *target) {
		target->setLimit(M_PI / 360.0f, M_PI / 1.2f);
		target->enableAngularMotor(true, -6.0, 4000.f);
	}

	void moveUp(btHingeConstraint *target) {
		target->setLimit(M_PI / 360.0f, M_PI / 1.2f);
		target->enableAngularMotor(true, 6.0, 4000.f);

	}

	void moveDown(btHingeConstraint *target) {
		target->setLimit(M_PI / 360.0f, M_PI / 1.2f);
		target->enableAngularMotor(true, -6.0, 4000.f);
	}

	short collisionFilterGroup = short(btBroadphaseProxy::CharacterFilter);
	short collisionFilterMask = short(btBroadphaseProxy::AllFilter ^ (btBroadphaseProxy::CharacterFilter));

	/********************************************************************************************
	* end custom Functions
	*********************************************************************************************/

};

ArmNNTest::ArmNNTest(struct GUIHelperInterface* helper)
	:CommonRigidBodyBase(helper),
	m_once(true)
{
	rl_ = new ArmRL();
	rl_sd_ = new ArmRL();
	rl_->init();
	rl_sd_->init();

	state_buffer_.initialize(1, true); // 1 = num of state
	state_buffer_.assignAllValues(2.0f);

	state_buffer_[0] = 2.0f; // 2.0f = max length of fist to head distance

	for (int h = 0; h < rl_->num_input_histories_; h++)
	{
		rl_->recordHistory(state_buffer_, 0.0f, 2, VectorND<float>(3)); // 3 = num of action
	}

	for (int h = 0; h < rl_sd_->num_input_histories_; h++)
	{
		rl_sd_->recordHistory(state_buffer_, 0.0f, 2, VectorND<float>(3)); // 3 = num of action
	}
}

ArmNNTest::~ArmNNTest()
{
	for (int i = 0; i<m_jointFeedback.size(); i++)
	{
		delete m_jointFeedback[i];
	}

}

void ArmNNTest::lockLiftHinge(btHingeConstraint* hinge)
{
	btScalar hingeAngle = hinge->getHingeAngle();
	btScalar lowLim = hinge->getLowerLimit();
	btScalar hiLim = hinge->getUpperLimit();
	hinge->enableAngularMotor(false, 0, 0);

	if (hingeAngle < lowLim)
	{
		//		m_liftHinge->setLimit(lowLim, lowLim + LIFT_EPS);
		hinge->setLimit(lowLim, lowLim);
	}
	else if (hingeAngle > hiLim)
	{
		//		m_liftHinge->setLimit(hiLim - LIFT_EPS, hiLim);
		hinge->setLimit(hiLim, hiLim);
	}
	else
	{
		//		m_liftHinge->setLimit(hingeAngle - LIFT_EPS, hingeAngle + LIFT_EPS);
		hinge->setLimit(hingeAngle, hingeAngle);
	}
	return;
}

void ArmNNTest::stepSimulation(float deltaTime)
{
	// get distance
	distance_ = sqrt(pow((body->getCenterOfMassPosition().getZ() - linkBody->getCenterOfMassPosition().getZ()), 2) + pow((body->getCenterOfMassPosition().getY() - linkBody->getCenterOfMassPosition().getY()), 2)) - 0.225;

	// make VectorND state
	state_buffer_[0] = distance_;

	// set reward
	// h529 : 거리가 가까울수록 칭찬
	float reward_ = (1 - (distance_ / 2.5f));
	if (reward_ < 0.2f) {
		reward_ = -0.1f;
	}

	// set checkEndLearningCycle
	bool checkEndLearningCycle = false;

	/********************************************************************************************
	* start Action
	*********************************************************************************************/

	// forward
	rl_->forward();
	VectorND<float> output_vector_temp;
	rl_->nn_.copyOutputVectorTo(false, output_vector_temp);
	VectorND<float> output_target_temp;

	// forward_sd

	rl_sd_->forward();
	VectorND<float> output_vector_temp_sd_;
	rl_->nn_.copyOutputVectorTo(false, output_vector_temp_sd_);
	VectorND<float> output_target_temp_sd_;

	// set Random percent
	float dice = chkModeStudying ? 0.1f : 0.0f;

	// decide the shoulder action
	int probability_shoulder = rl_sd_->nn_.getOutputIXEpsilonGreedy(dice); // fix only up
	int action_shoulder = -1;

	if (probability_shoulder == ACTION_SHOULDER_UP) {
		moveUp(hinge_shader);
		action_shoulder = ACTION_SHOULDER_UP;
	}
	else if (probability_shoulder == ACTION_SHOULDER_DOWN) {
		moveDown(hinge_shader);
		action_shoulder = ACTION_SHOULDER_DOWN;
	}
	else {
		action_shoulder = ACTION_SHOULDER_STAY;
	}

	// decide the elbow action
	int probability_elbow = rl_->nn_.getOutputIXEpsilonGreedy(dice);
	int action_elbow = -1;

	if (probability_elbow == ACTION_ELBOW_IN) {
		moveIn(hinge_elbow);
		action_elbow = ACTION_ELBOW_IN;
	}
	else if (probability_elbow == ACTION_ELBOW_OUT) {
		moveOut(hinge_elbow);
		action_elbow = ACTION_ELBOW_OUT;
	}
	else {
		action_elbow = ACTION_ELBOW_STAY;
	}

	/********************************************************************************************
	* end Action
	*********************************************************************************************/

	//collison check
	int numManifolds = m_dynamicsWorld->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++)
	{
		if (m_dynamicsWorld->getDispatcher()->getNumManifolds() == 0) continue;
		btPersistentManifold* contactManifold = m_dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject* obA = contactManifold->getBody0();
		const btCollisionObject* obB = contactManifold->getBody1();

		int numContacts = contactManifold->getNumContacts();
		for (int j = 0; j < numContacts; j++)
		{
			btManifoldPoint& pt = contactManifold->getContactPoint(j);
			if (pt.getDistance() < 0.1f)
			{
				const btVector3& ptA = pt.getPositionWorldOnA();
				const btVector3& ptB = pt.getPositionWorldOnB();
				const btVector3& normalOnB = pt.m_normalWorldOnB;
				//check the head or body
				if (distance_ <= sqrt(0.08))
				{
					// h529 : 새 보상값에 의해 강제 값은 필요없음
					// reward_ = 0.5f;
					checkEndLearningCycle = true;
				}
				else
				{
					reward_ = 0.0f;
					checkEndLearningCycle = true;
				}
			}
		}
	}

	// Memory current state
	rl_->recordHistory(state_buffer_, reward_, action_elbow, output_vector_temp);
	rl_sd_->recordHistory(state_buffer_, reward_, action_shoulder, output_vector_temp);

	// force reset
	if (rl_->memory_->num_elements_ > 300) {
		checkEndLearningCycle = true;
	}

	// print current state
	b3Printf("md(%s)\tact_sh: %d\tact_eb: %d\tdst: %f\trwd: %f\n", chkModeStudying ? "st" : "rn", action_shoulder, action_elbow, distance_, reward_);

	// if end LearningCycle, then it's time to tranning!
	if (checkEndLearningCycle) {
		/********************************************************************************************
		* start Trainning
		*********************************************************************************************/

		// print this cycle's info
		b3Printf("=======================================================================\n");
		b3Printf("steps(num_reserve) : %d\n", rl_->memory_->num_elements_);
		b3Printf("=======================================================================\n");

		// start trainning
		int tr_num = 10;

		// h529 : 인위적인 강화		
		if (distance_ < 0.7f) {
			tr_num += 50;
		}
		if (distance_ < 0.5f) {
			tr_num += 50;
		}
		if (distance_ < 0.3f) {
			tr_num += 50;
		}
		if (distance_ < 0.2f) {
			tr_num += 50;
		}

		// elbow
		if (chkModeStudying == true)
			for (int tr = 0; tr < tr_num; tr++)
				for (int m_tr = rl_->memory_->num_elements_ - 2; m_tr >= rl_->num_input_histories_; m_tr--)
				{
					// stochastic training
					// h529 : 전체를 요약한 부분을 확률적으로 선택해서 학습하는 방법론
					// h529 : http://sanghyukchun.github.io/74/
					int m = rand() % (rl_->memory_->num_elements_ - 1 - rl_->num_input_histories_) + rl_->num_input_histories_;

					// memory index from end
					const int inv_m = m - (rl_->memory_->num_elements_ - 1);

					float Q_next = 0.0f;
					if (m != rl_->memory_->num_elements_ - 2) // if next is not the terminal state
					{
						// Q_next = ...;
						Q_next = rl_->memory_->q_values_array_[m + 1].getMaxValue();
					}

					float Q_target;
					// Q_target = ...;
					Q_target = Q_next + rl_->memory_->reward_array_[m];

					// forward propagation from previous inputs
					rl_->makeInputVectorFromHistory(inv_m - 1, rl_->old_input_vector_);
					rl_->nn_.setInputVector(rl_->old_input_vector_);
					for (int i = 0; i < 100; i++)
					{
						rl_->nn_.feedForward();
						rl_->nn_.copyOutputVectorTo(false, output_target_temp);

						// output_target_temp[...] = ...;
						// h529 : Q_next와 Q_target으로 강화한 값을 새로 역전파시킬 준비
						output_target_temp[rl_->memory_->selected_array_[m]] = Q_target;

						rl_->nn_.propBackward(output_target_temp);
					}

					rl_->nn_.check();
				}

		// shoulder
		if (chkModeStudying == true)
			for (int tr = 0; tr < tr_num; tr++)
				for (int m_tr = rl_sd_->memory_->num_elements_ - 2; m_tr >= rl_sd_->num_input_histories_; m_tr--)
				{
					// stochastic training
					// h529 : 전체를 요약한 부분을 확률적으로 선택해서 학습하는 방법론
					// h529 : http://sanghyukchun.github.io/74/
					int m = rand() % (rl_sd_->memory_->num_elements_ - 1 - rl_sd_->num_input_histories_) + rl_sd_->num_input_histories_;

					// memory index from end
					const int inv_m = m - (rl_sd_->memory_->num_elements_ - 1);

					float Q_next = 0.0f;
					if (m != rl_sd_->memory_->num_elements_ - 2) // if next is not the terminal state
					{
						// Q_next = ...;
						Q_next = rl_sd_->memory_->q_values_array_[m + 1].getMaxValue();
					}

					float Q_target;
					// Q_target = ...;
					Q_target = Q_next + rl_sd_->memory_->reward_array_[m];

					// forward propagation from previous inputs
					rl_sd_->makeInputVectorFromHistory(inv_m - 1, rl_sd_->old_input_vector_);
					rl_sd_->nn_.setInputVector(rl_sd_->old_input_vector_);
					for (int i = 0; i < 100; i++)
					{
						rl_sd_->nn_.feedForward();
						rl_sd_->nn_.copyOutputVectorTo(false, output_target_temp);

						// output_target_temp[...] = ...;
						// h529 : Q_next와 Q_target으로 강화한 값을 새로 역전파시킬 준비
						output_target_temp[rl_sd_->memory_->selected_array_[m]] = Q_target;

						rl_sd_->nn_.propBackward(output_target_temp);
					}

					rl_sd_->nn_.check();
				}

		// reset & restart
		rl_->memory_->reset();
		rl_sd_->memory_->reset();
		initState(this);

		/********************************************************************************************
		* end Tranning
		*********************************************************************************************/
	}

	m_dynamicsWorld->stepSimulation(1. / 240, 0);

	static int count = 0;
}

void ArmNNTest::initPhysics()
{
	int upAxis = 1;
	m_guiHelper->setUpAxis(upAxis);

	createEmptyDynamicsWorld();
	m_dynamicsWorld->getSolverInfo().m_splitImpulse = false;

	m_dynamicsWorld->setGravity(btVector3(0, 0, -10));

	m_guiHelper->createPhysicsDebugDrawer(m_dynamicsWorld);
	int mode = btIDebugDraw::DBG_DrawWireframe
		+ btIDebugDraw::DBG_DrawConstraints
		+ btIDebugDraw::DBG_DrawConstraintLimits;
	m_dynamicsWorld->getDebugDrawer()->setDebugMode(mode);

	{ // create a door using hinge constraint attached to the world

		int numLinks = 3;
		bool spherical = false;					//set it ot false -to use 1DoF hinges instead of 3DoF sphericals
		bool canSleep = false;
		bool selfCollide = false;

		btVector3 baseHalfExtents(0.05, 0.37, 0.1);
		btVector3 linkHalfExtents(0.05, 0.37, 0.1);

		btBoxShape* baseBox = new btBoxShape(baseHalfExtents);
		btVector3 basePosition = btVector3(-0.4f, 4.f, 0.f);
		btTransform baseWorldTrans;
		baseWorldTrans.setIdentity();
		baseWorldTrans.setOrigin(basePosition);

		//mbC->forceMultiDof();							//if !spherical, you can comment this line to check the 1DoF algorithm
		//init the base
		btVector3 baseInertiaDiag(0.f, 0.f, 0.f);
		float baseMass = 0.f;
		float linkMass = 1.f;

		btRigidBody* base = createRigidBody(baseMass, baseWorldTrans, baseBox);
		m_dynamicsWorld->removeRigidBody(base);
		base->setDamping(0, 0);
		m_dynamicsWorld->addRigidBody(base, collisionFilterGroup, collisionFilterMask);
		btBoxShape* linkBox1 = new btBoxShape(linkHalfExtents);
		btBoxShape* linkBox2 = new btBoxShape(linkHalfExtents);
		btSphereShape* linkSphere = new btSphereShape(radius);

		btRigidBody* prevBody = base;

		for (int i = 0; i<numLinks; i++)
		{
			btTransform linkTrans;
			linkTrans = baseWorldTrans;

			linkTrans.setOrigin(basePosition - btVector3(0, linkHalfExtents[1] * 2.f*(i + 1), 0));

			btCollisionShape* colOb = 0;

			if (i == 0)
			{
				colOb = linkBox1;
			}
			else if (i == 1)
			{
				colOb = linkBox2;
			}
			else
			{
				colOb = linkSphere;
			}
			linkBody = createRigidBody(linkMass, linkTrans, colOb);
			m_dynamicsWorld->removeRigidBody(linkBody);
			m_dynamicsWorld->addRigidBody(linkBody, collisionFilterGroup, collisionFilterMask);
			linkBody->setDamping(0, 0);
			btTypedConstraint* con = 0;

			if (i == 0)
			{
				//create a hinge constraint
				btVector3 pivotInA(0, -linkHalfExtents[1], 0);
				btVector3 pivotInB(0, linkHalfExtents[1], 0);
				btVector3 axisInA(1, 0, 0);
				btVector3 axisInB(1, 0, 0);
				bool useReferenceA = true;
				hinge_shader = new btHingeConstraint(*prevBody, *linkBody,
					pivotInA, pivotInB,
					axisInA, axisInB, useReferenceA);
				hinge_shader->setLimit(0.0f, 0.0f);
				m_dynamicsWorld->addConstraint(hinge_shader, true);
				con = hinge_shader;
			}
			else if (i == 1)
			{
				//create a hinge constraint
				btVector3 pivotInA(0, -linkHalfExtents[1], 0);
				btVector3 pivotInB(0, linkHalfExtents[1], 0);
				btVector3 axisInA(1, 0, 0);
				btVector3 axisInB(1, 0, 0);
				bool useReferenceA = true;
				hinge_elbow = new btHingeConstraint(*prevBody, *linkBody,
					pivotInA, pivotInB,
					axisInA, axisInB, useReferenceA);
				hinge_elbow->setLimit(0.0f, 0.0f);
				m_dynamicsWorld->addConstraint(hinge_elbow, true);
				con = hinge_elbow;
			}
			else
			{
				btTransform pivotInA(btQuaternion::getIdentity(), btVector3(0, -radius, 0));						//par body's COM to cur body's COM offset
				btTransform pivotInB(btQuaternion::getIdentity(), btVector3(0, radius, 0));							//cur body's COM to cur body's PIV offset
				btGeneric6DofSpring2Constraint* fixed = new btGeneric6DofSpring2Constraint(*prevBody, *linkBody,
					pivotInA, pivotInB);
				fixed->setLinearLowerLimit(btVector3(0, 0, 0));
				fixed->setLinearUpperLimit(btVector3(0, 0, 0));
				fixed->setAngularLowerLimit(btVector3(0, 0, 0));
				fixed->setAngularUpperLimit(btVector3(0, 0, 0));

				con = fixed;

			}
			btAssert(con);
			if (con)
			{
				btJointFeedback* fb = new btJointFeedback();
				m_jointFeedback.push_back(fb);
				con->setJointFeedback(fb);

				m_dynamicsWorld->addConstraint(con, true);
			}
			prevBody = linkBody;

		}


	}

	if (1)
	{
		btVector3 groundHalfExtents(0.4, 0.0, 0.025);
		groundHalfExtents[upAxis] = 0.4f;
		btBoxShape* box = new btBoxShape(groundHalfExtents);
		box->initializePolyhedralFeatures();

		btTransform start; start.setIdentity();
		groundOrigin_target = btVector3(-0.4f, 4.0f, -1.45f);

		start.setOrigin(groundOrigin_target);
		body = createRigidBody(0, start, box);

		body->setFriction(0);



		btVector3 human_HalfExtents(0.8, 0.0, 0.025);
		human_HalfExtents[upAxis] = 0.8f;
		btBoxShape* human_box = new btBoxShape(human_HalfExtents);
		human_box->initializePolyhedralFeatures();

		btTransform human_start;
		human_start.setIdentity();
		groundOrigin_target = btVector3(-0.4f, 2.8f, -1.45f);

		human_start.setOrigin(groundOrigin_target);
		human_body = createRigidBody(0, human_start, human_box);

		human_body->setFriction(0);

	}

	m_guiHelper->autogenerateGraphicsObjects(m_dynamicsWorld);


	/********************************************************************************************
	* start init RL
	*********************************************************************************************/

	// initializeAI

	/********************************************************************************************
	* end init RL
	*********************************************************************************************/
}

bool ArmNNTest::keyboardCallback(int key, int state)
{
	bool handled = true;
	if (state)
	{
		switch (key)
		{
		case B3G_HOME:
		{
			// b3Printf("Rest.\n");
			initState(this);
			break;
		}
		case B3G_END:
		{
			chkModeStudying = chkModeStudying ? false : true;
			break;
		}
		case B3G_LEFT_ARROW:
		{
			// b3Printf("left.\n");
			moveIn(hinge_elbow);
			handled = true;
			break;

		}
		case B3G_RIGHT_ARROW:
		{
			// b3Printf("right.\n");
			moveOut(hinge_elbow);
			handled = true;
			break;
		}
		case B3G_UP_ARROW:
		{
			// b3Printf("left.\n");
			moveUp(hinge_shader);
			handled = true;
			/*btVector3 basePosition = btVector3(0.0, 0.0f, 1.54f);
			body->translate(basePosition);*/

			break;

		}
		case B3G_DOWN_ARROW:
		{
			// b3Printf("left.\n");
			moveDown(hinge_shader);
			handled = true;
			/*btVector3 basePosition = btVector3(0.0, 0.0f, -1.54f);
			body->translate(basePosition);*/
			break;
		}
		}
	}
	else
	{
		switch (key)
		{
		case B3G_LEFT_ARROW:
		case B3G_RIGHT_ARROW:
		{
			lockLiftHinge(hinge_elbow);
			handled = true;
			break;
		}
		case B3G_UP_ARROW:
		case B3G_DOWN_ARROW:
		{
			lockLiftHinge(hinge_shader);
			handled = true;
			break;
		}
		default:

			break;
		}
	}
	return handled;
}


CommonExampleInterface*    env_012(CommonExampleOptions& options)
{
	return new ArmNNTest(options.m_guiHelper);

}