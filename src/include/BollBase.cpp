#include "BollBase.h"

using namespace ison::indicator;

BollBase::BollBase()
{
}

BollBase::~BollBase()
{
}
BOLL_STORE_RetCode BollBase::Store(std::string kline_src)
{
	char srcBuffer[BUFFELENGTH];
	std::memcpy(srcBuffer, kline_src.c_str(), kline_src.size());
	baseline::MessageHeader hdr_src;
	baseline::SDS_Kline KK_src;
	hdr_src.wrap(srcBuffer + TOPICHEADSIZE, 0, MESSAGEHEADERVERSION, BUFFELENGTH);
	KK_src.wrapForDecode(srcBuffer + TOPICHEADSIZE, hdr_src.size(), hdr_src.blockLength(), hdr_src.version(), BUFFELENGTH);

	if ((KK_src.time() < 93000000) || (KK_src.time() > 113059999 && KK_src.time() < 130000000) || (KK_src.time() > 150059999))
	{
		return BOLL_ERROR;
	}

	std::map<std::string, std::vector<int>>::iterator it = DataMap.find(KK_src.code());
	if (it == DataMap.end())
	{
		std::vector<int> v_price;
		v_price.push_back(KK_src.close());
		DataMap.insert(std::pair<std::string, std::vector<int>>(KK_src.code(), v_price));
		return BOLL_ADD;
	}
	if (it->second.size() < DATACNTBOLL - 1)
	{
		it->second.push_back(KK_src.close());
		return BOLL_INSERT;
	}
	else
	{
		TA_Real    InReal[DATACNTBOLL];
		TA_Integer OutBeg;
		TA_Integer OutNbElement;
		TA_Real    outRealUpperBand[DATACNTBOLL];
		TA_Real    outRealMiddleBand[DATACNTBOLL];
		TA_Real    outRealLowerBand[DATACNTBOLL];
		int i = 0;
		it->second.push_back(KK_src.close());
		for (auto value : it->second)
		{
			InReal[i++] = value;
		}
		if (TA_BBANDS(0, DATACNTBOLL - 1, &InReal[0], TIMEPERIODBOLL, NBDEVUP, NBDEVDN, MATYPE, &OutBeg, &OutNbElement, &outRealUpperBand[0], &outRealMiddleBand[0], &outRealLowerBand[0]) == TA_SUCCESS)
		{
			ResultDataArr[0] = outRealMiddleBand[OutNbElement - 1];
			ResultDataArr[1] = outRealUpperBand[OutNbElement - 1];
			ResultDataArr[2] = outRealLowerBand[OutNbElement - 1];
			it->second.erase(it->second.begin());

			char storeBuffer[BUFFELENGTH];
			baseline::MessageHeader hdr_Store;
			baseline::SDS_Indicator II_Store;
			hdr_Store.wrap(storeBuffer, 0, MESSAGEHEADERVERSION, BUFFELENGTH)
				.blockLength(baseline::SDS_Indicator::sbeBlockLength())
				.templateId(baseline::SDS_Indicator::sbeTemplateId())
				.schemaId(baseline::SDS_Indicator::sbeSchemaId())
				.version(baseline::SDS_Indicator::sbeSchemaVersion());
			II_Store.wrapForEncode(storeBuffer, hdr_Store.size(), BUFFELENGTH)
				.signalID(MACD_SIGNALID)
				.putCode(KK_src.code())
				.date(KK_src.date())
				.time(KK_src.time());
			II_Store.dataArr(0, ResultDataArr[0]);
			II_Store.dataArr(1, ResultDataArr[1]);
			II_Store.dataArr(2, ResultDataArr[2]);
			II_Store.dataArr(3, 0);
			II_Store.dataArr(4, 0);
			SendStr.clear();
			SendStr.assign(storeBuffer, hdr_Store.size() + II_Store.size());

			return BOLL_REPLACE;
		}
		else
		{
			it->second.erase(it->second.begin());
			return BOLL_ERROR;
		}
	}
}

void BollBase::GetResult(double Result[])
{
	Result[0] = ResultDataArr[0];
	Result[1] = ResultDataArr[1];
	Result[2] = ResultDataArr[2];
}

std::string BollBase::MakeSendStr(int topic, int sn)
{
	char SendBuf[BUFFELENGTH];
	char Code[16];
	TOPICHEAD TopicHeadSend;
	baseline::MessageHeader HdrSend;
	baseline::SDS_Indicator II_send;

	std::memcpy(SendBuf, SendStr.c_str(), SendStr.size());
	HdrSend.wrap(SendBuf, 0, MESSAGEHEADERVERSION, BUFFELENGTH);
	II_send.wrapForDecode(SendBuf, HdrSend.size(), HdrSend.blockLength(), HdrSend.version(), BUFFELENGTH);

	TopicHeadSend.topic = topic;
	DateAndTime m_dtm = GetDateAndTime();
	TopicHeadSend.ms = (m_dtm.time % 1000);
	std::memcpy(Code, II_send.code(), 16);
	TopicHeadSend.kw = atoi(Code);
	TopicHeadSend.sn = sn;
	unsigned int num_tm;
	DateTime2Second(m_dtm.date, m_dtm.time, num_tm);
	TopicHeadSend.tm = num_tm;
	memcpy(SendBuf, &TopicHeadSend, sizeof(TOPICHEAD));

	std::string str_temp;
	str_temp.assign(SendBuf, sizeof(TOPICHEAD));
	str_temp.append(SendStr);
	return str_temp;
}

const char* BollBase::GetDataCode()const
{
	if (SendStr.size() > 0)
	{
		char Buffer[BUFFELENGTH];
		baseline::MessageHeader HdrSend;
		baseline::SDS_Indicator II;

		std::memcpy(Buffer, SendStr.c_str(), SendStr.size());
		HdrSend.wrap(Buffer, 0, MESSAGEHEADERVERSION, BUFFELENGTH);
		II.wrapForDecode(Buffer, HdrSend.size(), HdrSend.blockLength(), HdrSend.version(), BUFFELENGTH);

		return II.code();
	}
}