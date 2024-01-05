#pragma once

#include <windows.h>

#include <fstream>
#include <string>
#include <vector>

class Helper {
 public:
  static bool GetEnginePath(std::string& enginePath);

  static bool ReadData(const std::string& name, std::vector<BYTE>& blob);
};