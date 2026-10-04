#include "DevBenchTool.h"
#include "Page.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <SKSE/SKSE.h>

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	if (!a_msg) {
		return;
	}
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostPostLoad:
		page::Register();
		DevBenchTool::Init();
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		strings::Configure("HUDPositionManager");
		DevBenchTool::Init(/* a_lastAttempt = */ true);
		break;
	default:
		break;
	}
}
