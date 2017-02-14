#pragma once

#include <map>
#include <string>

#include "000_empty/000_enviroment.h"
#include "001_Single_Joint/001_enviroment.h"
#include "002_Two_Joint_Enviroment_Test/002_enviroment.h"
#include "003_Recorder/003_enviroment.h"
#include "010_Arm_Test/010_enviroment.h"
#include "011_Arm_4_Target/011_enviroment.h"
#include "012_Angle_Distance_Reward/012_enviroment.h"
#include "020_lab0_Enviroment/020_enviroment.h"
#include "021_lab0_Add_Angle/021_enviroment.h"
#include "030_Body_NN_Test/030_enviroment.h"
#include "040_Body_Enviroment_Test/040_enviroment.h"
#include "050_lab1_Enviroment/050_enviroment.h"
#include "051_lab1_F2TAD_Angle/051_enviroment.h"

struct ExampleImporter
{
	const char*		m_name = "";
	CommonExampleInterface::CreateFunc*  m_createFunc;

	ExampleImporter() {}

	ExampleImporter(const char* _name, CommonExampleInterface::CreateFunc* _createFunc) {
		m_name = _name;
		m_createFunc = _createFunc;
	}
};

static std::map<std::string, ExampleImporter> getExample = {
	{ "000", ExampleImporter("000_HelloWorld", env_000) },
	{ "001", ExampleImporter("001_Single_Joint", env_001) },
	{ "002", ExampleImporter("002_Two_Joint-Enviroment_Test", env_002) },
	{ "003", ExampleImporter("003_Recorder", env_003) },
	{ "010", ExampleImporter("010_Arm_Test", env_010) },
	{ "011", ExampleImporter("011_Arm_4_Target", env_011) },
	{ "012", ExampleImporter("012_Angle_Distance_Reward", env_012) },
	{ "020", ExampleImporter("020_lab0_Enviroment", env_020) },
	{ "021", ExampleImporter("021_lab0_Add_Angle", env_021) },
	{ "030", ExampleImporter("030_Body_NN_Test", env_030) },
	{ "040", ExampleImporter("040_Body_Enviroment_Test", env_040) },
	{ "050", ExampleImporter("050_lab1_Enviroment", env_050) },
	{ "051", ExampleImporter("051_lab1_F2TAD_Angle", env_051) },
};