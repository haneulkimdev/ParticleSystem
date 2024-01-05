#include "Helper.h"

bool Helper::GetEnginePath(std::string& enginePath) {
  char ownPth[2048];
  GetModuleFileNameA(nullptr, ownPth, sizeof(ownPth));
  std::string exe_path = ownPth;
  std::string exe_path_;
  size_t pos = 0;
  std::string token;
  std::string delimiter = "\\";
  while ((pos = exe_path.find(delimiter)) != std::string::npos) {
    token = exe_path.substr(0, pos);
    if (token.find(".exe") != std::string::npos) break;
    exe_path += token + "\\";
    exe_path_ += token + "\\";
    exe_path.erase(0, pos + delimiter.length());
  }

  std::ifstream file(exe_path_ + "..\\engine_module_path.txt");

  if (!file.is_open()) return false;

  std::getline(file, enginePath);

  file.close();

  return true;
}

bool Helper::ReadData(const std::string& name, std::vector<BYTE>& blob) {
  std::ifstream fin(name, std::ios::binary);

  if (!fin) return false;

  blob.assign((std::istreambuf_iterator<char>(fin)),
              std::istreambuf_iterator<char>());

  fin.close();

  return true;
}