#pragma once
#include <cstdlib>
#include <fstream>
#include <string>
#include <map>
#include <vector>

class Utils
{
public:
	/////////////////////////////////////////
	// Constructor
	/////////////////////////////////////////
	Utils() = delete;

public:
	/////////////////////////////////////////
	// .env
	/////////////////////////////////////////
	static bool EnvInit(const std::string& fileName = ".env");
	static std::string getEnv(const std::string& key);

	/////////////////////////////////////////
	// Load CSV file
	/////////////////////////////////////////
	static std::vector<std::vector<std::string>> LoadCSVFiles(int customer_id);

	/////////////////////////////////////////
	// Error Log Save
	/////////////////////////////////////////
	static void LogError(const std::string& errorMsg, const std::string& functionName, std::string fileName = "error_log_");
	static void TestLogError(const std::string& msg = "Test Error Log");

public:
	/////////////////////////////////////////
	// Time
	/////////////////////////////////////////
	static long long  GetNowTimeMs(); // 1673529600123
	static std::string GetNowTimeUtc9(); // %Y-%m-%d %H:%M:%S
	static std::string GetNowTImeHHMMDD();
	static std::string GetNowTImeYMD();

private:
	static std::map<std::string, std::string> _envVariables;
};
