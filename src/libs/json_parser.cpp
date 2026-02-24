#include "json_parser.h"

std::unique_ptr<Request> JsonParser::process(const std::string& frame) {
	json_error_t err;
	Json root(json_loads(frame.c_str(), 0, &err));
	if (!root) {
		throw runtime_errorf("Failed to parse JSON (%s): %s", frame.c_str(), err.text);
	}

	return std::make_unique<JsonRequest>(std::move(root));
}