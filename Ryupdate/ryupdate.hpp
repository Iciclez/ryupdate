#pragma once
#include "zephyrus.hpp"

class ryupdate
{
public:
	ryupdate();
	~ryupdate();

	static void initialize();
	static void destroy();
};

extern zephyrus z;