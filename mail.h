#pragma once

#include <chrono>
#include <string>
#include <vector>

struct mail {
	std::string filename;

	/* metadata */
	int64_t timestamp;
	std::string sender;
	std::vector<std::string> receipient;
	std::string subject;

	const bool operator<(const mail& left) {
		return this->timestamp < left.timestamp;
	}

	void remove();
};