#include "Utils.h"
#include <iomanip>
#include <ctime>
#include <sstream>
#include <chrono>
#include <windows.h>

#include <string>
#include <fstream>
#include <iostream>


std::map<std::string, std::string> Utils::_envVariables;

bool Utils::EnvInit(const std::string& fileName)
{
	TCHAR szCurrentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDir);

	// TCHAR -> std::wstring 변환
	std::wstring wideCurrentDir(szCurrentDir);
	std::string currentDir(wideCurrentDir.begin(), wideCurrentDir.end());

	// 실행 파일 디렉터리와 같은 위치에 있는 .env 파일 경로 생성
	std::string fullFilePath = currentDir + "\\" + fileName;

	std::ifstream envFile(fullFilePath);
	if (!envFile.is_open()) return false;

	std::string line;
	while (std::getline(envFile, line))
	{
		if (line.empty() || line[0] == '#') continue;

		size_t delimiterPos = line.find('=');
		if (delimiterPos != std::string::npos)
		{
			std::string key = line.substr(0, delimiterPos);
			std::string value = line.substr(delimiterPos + 1);
			_envVariables[key] = value;
		}
	}

	envFile.close();
	return true;
}

std::string Utils::getEnv(const std::string& key)
{
	auto it = _envVariables.find(key);
	if (it != _envVariables.end()) return it->second;
	return "";
}

std::vector<std::vector<std::string>> Utils::LoadCSVFiles(int customer_id)
{
	TCHAR szCurrentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDir);

	// TCHAR -> std::wstring 변환
	std::wstring wideCurrentDir(szCurrentDir);
	std::string currentDir(wideCurrentDir.begin(), wideCurrentDir.end());

	std::string relativePath = currentDir + "\\..\\hyundai-nest-project2\\csv\\"+ GetNowTImeYMD();

	// 절대 경로 변환
	char absolutePath[MAX_PATH];
	GetFullPathNameA(relativePath.c_str(), MAX_PATH, absolutePath, nullptr);
	//std::cout << "변환된 절대 경로: " << absolutePath << '\n';

	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	std::string searchPattern = std::string(absolutePath) + "\\" + std::to_string(customer_id) + "_buck_*.csv";
	hFind = FindFirstFileA(searchPattern.c_str(), &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		std::cerr << "No matching CSV files found in " << absolutePath << std::endl;
		return {};
	}

	std::string latestFile;
	FILETIME latestTime = { 0, 0 }; // 최신 파일의 수정 시간 저장

	do {
		std::string fileName = findFileData.cFileName;

		// 현재 파일의 수정 시간을 가져옴
		FILETIME ft = findFileData.ftLastWriteTime;
		if (CompareFileTime(&ft, &latestTime) > 0) {
			latestTime = ft;
			latestFile = fileName;
		}

	} while (FindNextFileA(hFind, &findFileData) != 0);

	FindClose(hFind);

	if (latestFile.empty()) {
		std::cerr << "No latest CSV file found." << std::endl;
		return {};
	}

	std::string latestFilePath = std::string(absolutePath) + "\\" + latestFile;
	std::cout << "선택된 최신 파일: " << latestFilePath << std::endl;

	// CSV 파일 읽기
	std::vector<std::vector<std::string>> csvData;
	std::ifstream file(latestFilePath);
	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << latestFilePath << std::endl;
		return {};
	}

	std::string line;
	while (std::getline(file, line)) {
		std::vector<std::string> row;
		std::stringstream ss(line);
		std::string cell;

		while (std::getline(ss, cell, ',')) {
			row.push_back(cell);
		}

		csvData.push_back(row);
	}
	file.close();

	return csvData;
}


void Utils::LogError(const std::string& errorMsg, const std::string& functionName, std::string fileName)
{
	TCHAR szCurrentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szCurrentDir);
	std::wstring wideCurrentDir(szCurrentDir);
	std::string currentDir(wideCurrentDir.begin(), wideCurrentDir.end());

	auto t = std::time(nullptr);
	std::tm tm = {};
	localtime_s(&tm, &t);
	std::ostringstream dateStream;
	dateStream << std::put_time(&tm, "%Y_%m_%d");

	std::string fullFilePath = currentDir + "\\log\\" + fileName + dateStream.str() + ".txt";
	std::ofstream logFile(fullFilePath, std::ios::app);
	if (!logFile.is_open()) return;

	std::ostringstream timeStream;
	timeStream << std::put_time(&tm, "%H:%M:%S");

	logFile << '\n';
	logFile << "[" << timeStream.str() << "] -> [" << functionName << "]" << '\n';
	logFile << errorMsg << '\n';
	logFile.close();
}

void Utils::TestLogError(const std::string& msg)
{
	try
	{
		throw std::runtime_error(msg);
	}
	catch (const std::exception& e)
	{
		Utils::LogError(e.what(), "TestLogError");
	}
}

long long Utils::GetNowTimeMs()
{
	auto now = std::chrono::system_clock::now();
	auto nowTimeT = std::chrono::system_clock::to_time_t(now);
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()
	).count();
}

std::string Utils::GetNowTimeUtc9()
{
	auto now = std::chrono::system_clock::now();
	auto nowTimeT = std::chrono::system_clock::to_time_t(now);
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
	std::tm localTime;
	localtime_s(&localTime, &nowTimeT);
	std::ostringstream utc9TimeStream;
	utc9TimeStream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << millis;
	auto utc9Time = utc9TimeStream.str();

	return utc9Time;
}

std::string Utils::GetNowTImeHHMMDD()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	std::ostringstream oss;
	oss << std::setw(2) << std::setfill('0') << st.wHour   // HH
		<< std::setw(2) << std::setfill('0') << st.wMinute // MM
		<< std::setw(2) << std::setfill('0') << st.wDay;   // DD

	return oss.str();
}

std::string Utils::GetNowTImeYMD()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	std::ostringstream oss;
	oss << st.wYear << "-"
		<< std::setw(2) << std::setfill('0') << st.wMonth << "-"
		<< std::setw(2) << std::setfill('0') << st.wDay;

	return oss.str();
}
