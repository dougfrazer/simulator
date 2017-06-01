#include <cstdlib>
#include <time.h>
#include <list>

static float rand_floats(){ return (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f; }
static float rand_float() { return (((float)rand() / (float)RAND_MAX)); }

static const int numMsPerSec = 1000;
static const int numMsPerMin = numMsPerSec * 60;
static const int numMsPerHour = numMsPerMin * 60;
static const int numMsPerDay = numMsPerHour * 24;

static int s_simulationTime = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
struct SetupData
{
	int avgSellCount;
	int avgBuyCount;
	int avgStackCount;
	int sellPriceVariance;
	int buyPriceVariance;
	int stackCountVariance;
	int numDays;
};
///////////////////////////////////////////////////////////////////////////////////////////////////
static SetupData s_setupData = 
{
	5000,
	5000,
	20,
	1000,
	1000,
	10,
	7
};
///////////////////////////////////////////////////////////////////////////////////////////////////
struct CommodityTransaction
{
	int timestamp;
	int intrinsicValue;
	int actualValue;
};
///////////////////////////////////////////////////////////////////////////////////////////////////
struct CommodityData
{
	int totalValue;
	int totalCount;
	int GetIntrinsicValue();
	std::list<CommodityTransaction> history;
};
///////////////////////////////////////////////////////////////////////////////////////////////////
int CommodityData::GetIntrinsicValue()
{

}
///////////////////////////////////////////////////////////////////////////////////////////////////
static CommodityData s_commodityData;
///////////////////////////////////////////////////////////////////////////////////////////////////
static void RecalculateValue()
{

}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateBuy(int count)
{
	int toSubtract = count > s_commodityData.totalCount ? s_commodityData.totalCount : count;
	s_commodityData.totalCount -= toSubtract;
	s_commodityData.totalValue -= toSubtract * s_commodityData.GetIntrinsicValue();

	CommodityTransaction transaction;
	transaction.timestamp = s_simulationTime;
	transaction.intrinsicValue = s_commodityData.GetIntrinsicValue();
	transaction.actualValue = (int)((float)s_commodityData.totalValue / (float)s_commodityData.totalCount);
	s_commodityData.history.push_back(transaction);

	RecalculateValue();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateSell(int count)
{
	s_commodityData.totalCount += count;
	s_commodityData.totalValue += count * s_commodityData.GetIntrinsicValue();

	CommodityTransaction transaction;
	transaction.timestamp = s_simulationTime;
	transaction.intrinsicValue = s_commodityData.GetIntrinsicValue();
	s_commodityData.history.push_back(transaction);

	RecalculateValue();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{
	srand(time(NULL));

	s_commodityData.totalCount = 1000;
	s_commodityData.totalValue = s_commodityData.totalCount * s_commodityData.GetIntrinsicValue();

	CommodityTransaction transaction;
	transaction.intrinsicValue = 10;
	for (int i = 0; i < 1000; i++)
	{
		transaction.timestamp = s_simulationTime;
		s_simulationTime += numMsPerHour;
		s_commodityData.history.push_back(transaction);
	}

	const int numBuys = s_setupData.avgBuyCount + rand_floats() * s_setupData.buyPriceVariance;
	const int numSells = s_setupData.avgSellCount + rand_floats() * s_setupData.sellPriceVariance;
	const int totalExchanges = numBuys + numSells;
	const float buyChance = (float)(numBuys) / (float)(totalExchanges);
	
	const int numMsTotal = numMsPerDay * s_setupData.numDays;
	const int numMsPerTrade = (int)((float)numMsTotal / (float)totalExchanges);

	int currNumBuys = numBuys;
	int currNumSells = numSells;

	while (currNumBuys || currNumSells)
	{
		s_simulationTime += numMsPerTrade;
		const int count = s_setupData.avgStackCount + rand_floats() * s_setupData.stackCountVariance;
		if (!currNumSells || (currNumBuys && rand_float() < buyChance))
		{
			SimulateBuy(count);
			--currNumBuys;
		}
		else
		{
			SimulateSell(count);
			--currNumSells;
		}
	}
}