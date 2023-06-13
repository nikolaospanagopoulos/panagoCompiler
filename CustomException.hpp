#pragma once
#include <string>

#include <exception>
class CustomException : public std::exception {
  std::string message;

public:
  const char *what() { return message.c_str(); }
  CustomException(std::string errorMessage) : message(errorMessage){};
};
