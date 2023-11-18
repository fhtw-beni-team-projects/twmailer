#pragma once

#include "user_handler.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct mail {
	std::string filename;

	/* metadata */
	u_int id;
	int64_t timestamp;
	std::string subject;
	std::string sender;
	std::string recipient;

	bool deleted;

	mail(std::string filename, std::string subject, std::string sender);
	mail(std::string filename, int64_t timestamp, std::string subject);

	bool operator()(const u_int& id) const {
		return id == this->id;
	}

	bool operator<(mail& left) const {
		return left.timestamp > this->timestamp;
	}

	json mailToJson();

	void remove();

	std::mutex m_file;
};