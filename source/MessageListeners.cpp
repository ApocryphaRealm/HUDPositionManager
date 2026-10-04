#include "DevBenchTool.h"
#include "Immersive.h"
#include "Positioner.h"
#include "ActorBars.h"
#include "Page.h"
#include "Widgets.h"
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
		immersive::Register();
		positioner::RegisterAuthorApi();
		actorbars::Register();
		DevBenchTool::Init(/* a_lastAttempt = */ true);
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		widgets::SetGameReady(false);
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		widgets::SetGameReady(true);
		immersive::OnGameLoaded();
		break;
	default:
		break;
	}
}
