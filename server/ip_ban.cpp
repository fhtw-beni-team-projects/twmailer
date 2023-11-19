#include "ip_ban.h"

#include <chrono>
#include <filesystem>
#include <iterator>
#include <fstream>

ip_ban::ip_ban() : m_ban()
{
	this->file = fs::path();
}

ip_ban::~ip_ban()
{
	if (this->file.empty())
		return;

	json jsonfile = this->ban_list;

	std::ofstream ofs(this->file, std::ofstream::out | std::ofstream::trunc);
	ofs << jsonfile.dump();
}

void ip_ban::loadFile(fs::path file)
{
	this->file = file;

	if (fs::exists(file)) {
		std::ifstream ifs(file);
		this->ban_list = json::parse(ifs);
	}
}

void ip_ban::failedAttempt(std::string username, std::string ip)
{
	std::unique_lock<std::shared_mutex> lock(this->m_ban);

	std::map<std::string, std::pair<std::map<std::string, ushort>, time_t>>::iterator it = this->ban_list.insert({ip, {{}, 0}}).first;
	if (++it->second.first.insert({username, 0}).first->second >= MAX_ATTEMPTS) { // increase attempt count && check if reached MAX_ATTEMPTS
		for (auto& pair : it->second.first ) { // reset attempts to 0 for all names
			pair.second = 0;
		}
		(*it).second.second = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + BAN_TIME; // set unban time to current time + BAN_TIME seconds
	}
}

void ip_ban::success(std::string ip)
{
	std::unique_lock<std::shared_mutex> lock(this->m_ban);
	this->ban_list.erase(ip);
}

bool ip_ban::checkBanned(std::string ip)
{
	std::shared_lock<std::shared_mutex> lock(this->m_ban);

	std::map<std::string, std::pair<std::map<std::string, ushort>, time_t>>::iterator it;
	return (it = this->ban_list.find(ip)) != this->ban_list.end() && (*it).second.second > std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}