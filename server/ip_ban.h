#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <chrono>

#include <nlohmann/json.hpp>

#define BAN_TIME 60
#define MAX_ATTEMPTS 3

namespace fs = std::filesystem;

using json = nlohmann::json;

class ip_ban {
public:

	static ip_ban& getInstance() {
		static ip_ban instance;
		return instance;
	};

	ip_ban(ip_ban const&) = delete;
	ip_ban(ip_ban&&) = delete;
	ip_ban& operator=(ip_ban const&) = delete;
	ip_ban& operator=(ip_ban &&) = delete;

	void loadFile(fs::path file);

	void failedAttempt(std::string username, std::string ip);
	bool checkBanned(std::string ip);

protected:

	ip_ban();
	~ip_ban();

	std::shared_mutex m_ban;

	fs::path file;
	std::map<std::string, std::pair<std::map<std::string, ushort>, time_t>> ban_list;
	
};