#pragma once

#include <libsolidity/Node.h>
#include <string>

std::string compileFile(std::string name);
std::string compileCode(std::string name, std::string code);
std::string compileNode();
std::string compileNodeAbi();
std::string nodeAddress();


