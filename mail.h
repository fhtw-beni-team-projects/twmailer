#pragma once

#include "user_handler.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

struct mail {
	std::string filename;

	/* metadata */
	u_int id;
	int64_t timestamp;
	std::string sender;
	std::vector<std::string> recipients;
	std::string subject;

	bool operator()(const u_int& id) const {
		return id == this->id;
	}

	bool operator<(const mail& left) const {
		return this->timestamp < left.timestamp;
	}

	fs::path getPath() {
		if (this->filename.empty())
			return fs::path();
		return fs::path(this->filename.insert(2, "/"));
	};

	void remove() {
		if (this->filename.empty())
			return;
		std::remove((user_handler::getInstance()->getSpoolDir()/"objects"/fs::path(this->filename.insert(2, "/"))).c_str());

		this->filename = "";
	};
};