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
	return numMsPerMin;
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
};
//----------------------------------------------------------------------
// TODO: this is fake record data for the three commodities we're hackathoning
static CommodityData s_commodityData[NUM_COMMODITIES] =
{
	// ID = 1
	{
		123919,
	},
	// ID = 2
	{
		124437,
	},
	// ID = 3
	{
		124113,
	},
};
//----------------------------------------------------------------------
enum TRANSACTION_TYPE
{
	TRANS_BUY,
	TRANS_SELL,
};
//----------------------------------------------------------------------
// This is an individual auction up for sale
struct CommodityTransaction
{
	TRANSACTION_TYPE type;
	float cpv;
	float volatility;
	int totalCount;
	float totalValue;
	int amount;
	int64_t timestamp;
};
//----------------------------------------------------------------------
// This is a container for a single commodity, containing all the auctions, and some metadata
#ifdef SIMULATOR
typedef std::list<CommodityTransaction> CommodityHistory;
#else
typedef blz::list<CommodityTransaction> CommodityHistory;
#endif
struct PendingCommodityData
{
	int itemID;
	float currentPerUnitValue;
	float errorBars;
	//float volatility;
	float totalValue;
	int totalCount;
	CommodityHistory history;
	//----------------------------------------------------------------------
	void AddTransaction(int64_t timestamp, TRANSACTION_TYPE type, int amount)
	{
		CommodityTransaction transaction;
		transaction.timestamp = timestamp;
		transaction.totalValue = totalValue;
		transaction.cpv = currentPerUnitValue;
		transaction.totalCount = totalCount;
		transaction.type = type;
		transaction.amount = amount;
		transaction.volatility = errorBars;
		history.push_front(transaction);
	}
	//----------------------------------------------------------------------
	float GetRateOfInventoryChange(int numMs)
	{
		int stockNow = totalCount;
		int64_t timeNow = s_simulationTime;
		int stockTimeAgo = totalCount;
		int64_t timeAgo = s_simulationTime;
		bool foundEnd = false;
		for (const auto& node : history)
		{
			if (s_simulationTime - numMs > node.timestamp)
			{
				foundEnd = true;
				break; // only consider the time period considered
			}
			stockTimeAgo = node.totalCount;
			timeAgo = node.timestamp;
		}
		// This is so that if you do not have sufficient data, assume constant
		if (!foundEnd)
		{
			return 0.0f;
		}

		const int stockChange = stockNow - stockTimeAgo;
		const int64_t timeChangeMs = timeNow - timeAgo;
		const float rateOfChange = ((float)stockChange) / ((float)timeChangeMs);
		return rateOfChange;
	}
	//----------------------------------------------------------------------
	void RecalculateValue(int64_t)
	{
		const int timePeriod = GetTimePeriod();
		float rateOfChange = GetRateOfInventoryChange(timePeriod);
		
		// the magnitude represents the amount losing per timeperiod
		if (rateOfChange)
		{
			float magnitudeOfChange = rateOfChange*timePeriod;
			float percentageOfStockChangeOverTimePeriod = magnitudeOfChange / totalCount;
			if (percentageOfStockChangeOverTimePeriod > FLT_EPSILON)
			{
				errorBars = MIN(percentageOfStockChangeOverTimePeriod, 0.05f);

				currentPerUnitValue = MAX(currentPerUnitValue * (1.0f - percentageOfStockChangeOverTimePeriod), GetMinValue());
			}
		}
	}
};
//----------------------------------------------------------------------
// this is a container for all the commodities
static PendingCommodityData s_pendingCommodities[NUM_COMMODITIES];

///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateBuy(int listID, int count)
{
	int64_t prevTime = 0;
	if (s_pendingCommodities[listID].history.size())
	{
		prevTime = s_pendingCommodities[listID].history.front().timestamp;
	}
	int toSubtract = count > s_pendingCommodities[listID].totalCount ? s_pendingCommodities[listID].totalCount : count;
	s_pendingCommodities[listID].totalCount -= toSubtract;
	s_pendingCommodities[listID].totalValue -= toSubtract * s_pendingCommodities[listID].currentPerUnitValue;

	s_pendingCommodities[listID].AddTransaction(s_simulationTime, TRANS_BUY, count);

	if (prevTime)
	{
		s_pendingCommodities[listID].RecalculateValue(s_simulationTime - prevTime);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateSell(int listID, int count)
{
	int64_t prevTime = 0;
	if (s_pendingCommodities[listID].history.size())
	{
		prevTime = s_pendingCommodities[listID].history.front().timestamp;
	}

	s_pendingCommodities[listID].totalCount += count;
	s_pendingCommodities[listID].totalValue += count * s_pendingCommodities[listID].currentPerUnitValue;

	s_pendingCommodities[listID].AddTransaction(s_simulationTime, TRANS_SELL, count);

	if (prevTime)
	{
		s_pendingCommodities[listID].RecalculateValue(s_simulationTime - prevTime);
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
	const int numMsPerOrder = Fast_ftolRound((float)numMsForPeriod / (float)numTransaction);

	int currNumSells = numSells;
	int currNumBuys = numBuys;

	while (currNumSells && currNumBuys)
	{
		int stackSize = params.avgStackSize + Fast_ftolRound(rand_floats() * (params.stackSizeVariance));
		int listID = (int)(rand_float() * NUM_COMMODITIES);
		s_simulationTime += numMsPerOrder;

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
}
///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef SIMULATOR
int main()
{
	JamCommoditySimulate params;
	params.avgBuyCount = 10000;
	params.avgSellCount = 100;
	params.avgStackSize = 10;
	params.buyCountVariance = 0;
	params.sellCountVariance = 0;
	params.stackSizeVariance = 0;
	params.numDays = 1;

	s_simulationTime = time(NULL);

	for (int i = 0; i < NUM_COMMODITIES; i++)
	{
		const int costPerUnit = 10 * 10000;
		const int numUnits = 10000;
		s_pendingCommodities[i].itemID = s_commodityData[i].itemID;
		s_pendingCommodities[i].totalCount = numUnits;
		s_pendingCommodities[i].totalValue = costPerUnit * numUnits;
		s_pendingCommodities[i].errorBars = 0.0f;
		s_pendingCommodities[i].currentPerUnitValue = costPerUnit;
	}

	GlobalCommodityMarket_Simulate(params);
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////


