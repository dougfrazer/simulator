#include <cstdlib>
#include <time.h>
#include <list>
#include <cstdint>

static float rand_floats(){ return (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f; }
static float rand_float() { return (((float)rand() / (float)RAND_MAX)); }

static const int numMsPerSec = 1000;
static const int numMsPerMin = numMsPerSec * 60;
static const int numMsPerHour = numMsPerMin * 60;
static const int numMsPerDay = numMsPerHour * 24;
static const int numMsPerWeek = numMsPerHour * 24;

static int64_t s_simulationTime = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
static const int s_setup_avgSellCount          = 50;
static const int s_setup_avgBuyCount           = 5;
static const int s_setup_avgStackCount         = 20;
static const int s_setup_sellPriceVariance     = 0;
static const int s_setup_buyPriceVariance      = 0;
static const int s_setup_stackCountVariance    = 10;
static const int s_setup_numDays               = 1;
///////////////////////////////////////////////////////////////////////////////////////////////////
enum TRANSACTION_TYPE
{
	TRANS_BUY,
	TRANS_SELL,
};
///////////////////////////////////////////////////////////////////////////////////////////////////
struct CommodityTransaction
{
	int64_t totalValue;
	float cpv;
	TRANSACTION_TYPE type;
	int amount;
	int64_t timestamp;
};
///////////////////////////////////////////////////////////////////////////////////////////////////
struct CommodityData
{
	int totalValue;
	int totalCount;
	float currentPerUnitValue;
	void RecalculateValue();
	void AddTransaction(int64_t timestamp, TRANSACTION_TYPE type, int amount);
	std::list<CommodityTransaction> history;
private:
	int GetAvgTotalValue(int numMs);
};
///////////////////////////////////////////////////////////////////////////////////////////////////
static CommodityData s_commodityData;
///////////////////////////////////////////////////////////////////////////////////////////////////
void CommodityData::AddTransaction(int64_t timestamp, TRANSACTION_TYPE type, int amount)
{
	CommodityTransaction transaction;
	transaction.timestamp = timestamp;
	transaction.totalValue = s_commodityData.totalValue;
	transaction.cpv = s_commodityData.currentPerUnitValue;
	transaction.type = type;
	transaction.amount = amount;
	s_commodityData.history.push_front(transaction);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int CommodityData::GetAvgTotalValue(int numMs)
{
	int64_t value = totalValue;
	int count = 1;
	for (auto& node : s_commodityData.history)
	{
		if (s_simulationTime - numMs > node.timestamp)
		{
			break; // only consider the past day
		}
		value += node.totalValue;
		++count;
	}
	return value / count;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void CommodityData::RecalculateValue()
{
	int avgTotalValueForPastDay = GetAvgTotalValue(numMsPerDay);
	currentPerUnitValue = (float)avgTotalValueForPastDay / (float)totalCount;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateBuy(int count)
{
	int toSubtract = count > s_commodityData.totalCount ? s_commodityData.totalCount : count;
	s_commodityData.totalCount -= toSubtract;
	s_commodityData.totalValue -= toSubtract * s_commodityData.currentPerUnitValue;

	s_commodityData.AddTransaction(s_simulationTime, TRANS_BUY, count);

	s_commodityData.RecalculateValue();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
static void SimulateSell(int count)
{
	s_commodityData.totalCount += count;
	s_commodityData.totalValue += count * s_commodityData.currentPerUnitValue;

	s_commodityData.AddTransaction(s_simulationTime, TRANS_SELL, count);

	s_commodityData.RecalculateValue();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{
	srand(time(NULL));

	s_commodityData.totalCount = 1000;
	s_commodityData.currentPerUnitValue = 10;
	s_commodityData.totalValue = s_commodityData.totalCount * s_commodityData.currentPerUnitValue;
	s_simulationTime = 0;

	const int numBuys = s_setup_avgBuyCount + rand_floats() * s_setup_buyPriceVariance;
	const int numSells = s_setup_avgSellCount + rand_floats() * s_setup_sellPriceVariance;
	const int totalExchanges = numBuys + numSells;
	const float buyChance = (float)(numBuys) / (float)(totalExchanges);
	
	const int numMsTotal = numMsPerDay * s_setup_numDays;
	const int numMsPerTrade = (int)((float)numMsTotal / (float)totalExchanges);

	int currNumBuys = numBuys;
	int currNumSells = numSells;

	while (currNumBuys || currNumSells)
	{
		s_simulationTime += numMsPerTrade;
		const int count = s_setup_avgStackCount + rand_floats() * s_setup_stackCountVariance;
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