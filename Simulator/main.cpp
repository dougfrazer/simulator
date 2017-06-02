//#include "pch.h"

#define SIMULATOR 1

#ifdef SIMULATOR
#include <cstdint>
#include <time.h>
#include <list>
#else
#include "GlobalCommodityMarket.h"
#include "JamSharedAutoCode/JamMiscStruct.h"
#include "Tempest/crandom.h"
#endif

//----------------------------------------------------------------------
static const int numMsPerSec = 1000;
static const int numMsPerMin = numMsPerSec * 60;
static const int numMsPerHour = numMsPerMin * 60;
static const int numMsPerDay = numMsPerHour * 24;
static const int numMsPerWeek = numMsPerHour * 24;
static const int NUM_COMMODITIES = 3;
static int64_t s_simulationTime;
static const int simulationHeartBeatRate = 10000;
//----------------------------------------------------------------------
#ifdef SIMULATOR
static const int s_minValue = 2;
#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)
int Fast_ftolRound(float in)
{
	return (int)in;
}
struct JamCommoditySimulate
{
public:
	int numDays;
	int avgSellCount;
	int avgBuyCount;
	int avgStackSize;
	int sellCountVariance;
	int buyCountVariance;
	int stackSizeVariance;
};
#else
static CRndSeed s_rndSeed;
static CVar* s_cantGoBelowCvar;
static CVar* s_commodityTimePeriodCvar;
#endif
//----------------------------------------------------------------------
static float rand_floats()
{
#ifdef SIMULATOR
	return (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f;
#else
	return CRandom::floats_(s_rndSeed);
#endif
}
//----------------------------------------------------------------------
static float rand_float()
{
#ifdef SIMULATOR
	return (((float)rand() / (float)RAND_MAX));
#else
	return CRandom::float_(s_rndSeed);
#endif
}
//----------------------------------------------------------------------
static int GetMinValue()
{
#ifdef SIMULATOR
	return s_minValue;
#else
	return s_cantGoBelowCvar->GetInt();
#endif
}
//----------------------------------------------------------------------
static int GetTimePeriod()
{
#ifdef SIMULATOR
	return numMsPerHour;
#else
	return s_commodityTimePeriodCvar->GetInt();
#endif
}
//----------------------------------------------------------------------
// TODO: this is effectively a fake record format where we will describe
//       parameters of each commodity and how it should change over time
struct CommodityData
{
	int itemID;
	int baseValue; // in copper
};
//----------------------------------------------------------------------
// TODO: this is fake record data for the three commodities we're hackathoning
static CommodityData s_commodityData[NUM_COMMODITIES] =
{
	// ID = 1
	{
		123919,
		20000,
	},
	// ID = 2
	{
		124437,
		20000,
	},
	// ID = 3
	{
		124113,
		20000,
	},
};
//----------------------------------------------------------------------
// This is an individual auction up for sale
struct CommodityHistoryNode
{
	int totalCount;
	float volatility;
	float costPer;
	int64_t timestamp;
};
//----------------------------------------------------------------------
// This is a container for a single commodity, containing all the auctions, and some metadata
#ifdef SIMULATOR
typedef std::list<CommodityHistoryNode> CommodityHistory;
#else
typedef blz::list<CommodityTransaction> CommodityHistory;
#endif
struct PendingCommodityData
{
	int itemID;
	int totalCount;
	float costPer;
	float volatility;

	CommodityHistory history;
	//----------------------------------------------------------------------
	CommodityHistoryNode& Record()
	{
		CommodityHistoryNode historyNode;
		historyNode.timestamp = s_simulationTime;
		historyNode.totalCount = totalCount;
		historyNode.volatility = volatility;
		historyNode.costPer = costPer;
		history.push_front(historyNode);
		return history.front();
	}
	//----------------------------------------------------------------------
};
//----------------------------------------------------------------------
// this is a container for all the commodities
static PendingCommodityData s_pendingCommodities[NUM_COMMODITIES];

///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateBuy(int listID, int count)
{
	auto& commodity = s_pendingCommodities[listID];
	int toSubtract = count > commodity.totalCount ? commodity.totalCount : count;
	commodity.totalCount -= toSubtract;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateSell(int listID, int count)
{
	auto& commodity = s_pendingCommodities[listID];
	commodity.totalCount += count;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateHeartbeat(int listID)
{
	auto& commodity = s_pendingCommodities[listID];
	CommodityHistoryNode& newNode = commodity.Record();
	const int timePeriodMs = GetTimePeriod();

	// compute the rate at which we are selling our goods
	const CommodityHistoryNode* beforeNode = nullptr;
	for (auto& node : commodity.history)
	{
		if (s_simulationTime - timePeriodMs > node.timestamp)
		{
			beforeNode = &node;
			break; // only consider the time period considered
		}
	}
	if (!beforeNode)
	{
		// not enough data in our history, nothing to do
		return;
	}

	const int countChange = newNode.totalCount - beforeNode->totalCount;
	const int64_t timeChangeMs = newNode.timestamp - beforeNode->timestamp;
	const float rateOfChange = ((float)countChange) / ((float)timeChangeMs);
	const float changeByNextTick = rateOfChange * simulationHeartBeatRate;

	// compute a new price to match the demand
	// based on how we assume it to be trending, we want to make the price aim towards the same value as it currently is
	// so we will need to adjust prices to maintain the same value
	if (commodity.totalCount)
	{
		const float expectedCount = commodity.totalCount + changeByNextTick;
		commodity.volatility = 1.0f - ((commodity.totalCount + rateOfChange * timePeriodMs) / commodity.totalCount);
		const float currentValue = commodity.costPer * commodity.totalCount;
		const float expectedValue = commodity.costPer * expectedCount;
		const float expectedCostPer = expectedValue / commodity.totalCount;
		commodity.costPer = expectedCostPer;
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void GlobalCommodityMarket_Simulate(const JamCommoditySimulate& params)
{
	const int numSells = params.avgSellCount + Fast_ftolRound(rand_floats() * (params.sellCountVariance));
	const int numBuys = params.avgBuyCount + Fast_ftolRound(rand_floats() * (params.buyCountVariance));
	const int numTransaction = numSells + numBuys;
	const float buyChance = (float)(numBuys) / (float)(numTransaction);
	
	const int numMsForPeriod = params.numDays * numMsPerDay;
	const int numMsPerTransaction = Fast_ftolRound((float)numMsForPeriod / (float)numTransaction);
	int64_t lastTransTimestamp = 0;
	s_simulationTime = 0;
	int currNumSells = numSells;
	int currNumBuys = numBuys;
	while (s_simulationTime < numMsForPeriod)
	{
		for (int i = 0; i < NUM_COMMODITIES; i++)
		{
			SimulateHeartbeat(i);
		}
		
		const bool shouldProcessTransaction = (s_simulationTime - lastTransTimestamp) > numMsPerTransaction;
		if (shouldProcessTransaction)
		{
			lastTransTimestamp = s_simulationTime;
			int stackSize = params.avgStackSize + Fast_ftolRound(rand_floats() * (params.stackSizeVariance));
			int listID = (int)(rand_float() * NUM_COMMODITIES);

			// execute either a buy or a sell based on the ratio, trying to even split them up
			if (!currNumSells || (currNumBuys && rand_float() < buyChance))
			{
				SimulateBuy(listID, stackSize);
				--currNumBuys;
			}
			else
			{
				SimulateSell(listID, stackSize);
				--currNumSells;
			}
		}
		s_simulationTime += simulationHeartBeatRate;
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef SIMULATOR
int main()
{
	JamCommoditySimulate params;
	params.avgBuyCount = 100;
	params.avgSellCount = 1000;
	params.avgStackSize = 10;
	params.buyCountVariance = 0;
	params.sellCountVariance = 0;
	params.stackSizeVariance = 0;
	params.numDays = 1;

	s_simulationTime = time(NULL);

	for (int i = 0; i < NUM_COMMODITIES; i++)
	{
		s_pendingCommodities[i].costPer = s_commodityData[i].baseValue;
		s_pendingCommodities[i].totalCount = 10000;
	}

	GlobalCommodityMarket_Simulate(params);
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////


