/*
 *	Copyright (C) 2015,2016 Jonathan Naylor, G4KLX
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 */

#include "DMRControl.h"
#include "DMRAccessControl.h"
#include "Defines.h"
#include "DMRCSBK.h"
#include "Log.h"

#include <cstdio>
#include <cassert>
#include <algorithm>

CDMRControl::CDMRControl(unsigned int id, unsigned int colorCode, unsigned int callHang, bool selfOnly, const std::vector<unsigned int>& prefixes, const std::vector<unsigned int>& blacklist, const std::vector<unsigned int>& dstIdBlacklistSlot1RF, const std::vector<unsigned int>& dstIdWhitelistSlot1RF, const std::vector<unsigned int>& dstIdBlacklistSlot2RF, const std::vector<unsigned int>& dstIdWhitelistSlot2RF, const std::vector<unsigned int>& dstIdBlacklistSlot1NET, const std::vector<unsigned int>& dstIdWhitelistSlot1NET, const std::vector<unsigned int>& dstIdBlacklistSlot2NET, const std::vector<unsigned int>& dstIdWhitelistSlot2NET, unsigned int timeout, CModem* modem, CDMRNetwork* network, CDisplay* display, bool duplex, CDMRLookup* lookup, int rssiMultiplier, int rssiOffset, unsigned int jitter, bool tgRewriteSlot1, bool tgRewriteSlot2, bool bmAutoRewrite, bool bmRewriteReflectorVoicePrompts) :
m_id(id),
m_colorCode(colorCode),
m_modem(modem),
m_network(network),
m_slot1(1U, timeout),
m_slot2(2U, timeout),
m_lookup(lookup)
{
	assert(id != 0U);
	assert(modem != NULL);
	assert(display != NULL);
	assert(lookup != NULL);

	// Load black and white lists to DMRAccessControl
	CDMRAccessControl::init(dstIdBlacklistSlot1RF, dstIdWhitelistSlot1RF, dstIdBlacklistSlot2RF, dstIdWhitelistSlot2RF, dstIdBlacklistSlot1NET, dstIdWhitelistSlot1NET, dstIdBlacklistSlot2NET, dstIdWhitelistSlot2NET, blacklist, selfOnly, prefixes, id, callHang, tgRewriteSlot1, tgRewriteSlot2, bmAutoRewrite, bmRewriteReflectorVoicePrompts);

	CDMRSlot::init(colorCode, callHang, modem, network, display, duplex, m_lookup, rssiMultiplier, rssiOffset, jitter);
}

CDMRControl::~CDMRControl()
{
}

bool CDMRControl::processWakeup(const unsigned char* data)
{
	assert(data != NULL);

	// Wakeups always come in on slot 1
	if (data[0U] != TAG_DATA || data[1U] != (DMR_IDLE_RX | DMR_SYNC_DATA | DT_CSBK))
		return false;

	CDMRCSBK csbk;
	bool valid = csbk.put(data + 2U);
	if (!valid)
		return false;

	CSBKO csbko = csbk.getCSBKO();
	if (csbko != CSBKO_BSDWNACT)
		return false;

	unsigned int srcId = csbk.getSrcId();
	unsigned int bsId  = csbk.getBSId();

	std::string src = m_lookup->find(srcId);

	bool ret = CDMRAccessControl::validateSrcId(srcId);
	if (!ret) {
		LogMessage("Invalid CSBK BS_Dwn_Act received from %s", src.c_str());
		return false;
	}

	if (bsId == 0xFFFFFFU) {
		LogMessage("CSBK BS_Dwn_Act for ANY received from %s", src.c_str());
		return true;
	} else if (bsId == m_id) {
		LogMessage("CSBK BS_Dwn_Act for %u received from %s", bsId, src.c_str());
		return true;
	}

	return false;
}

void CDMRControl::writeModemSlot1(unsigned char *data, unsigned int len)
{
	assert(data != NULL);

	m_slot1.writeModem(data, len);
}

void CDMRControl::writeModemSlot2(unsigned char *data, unsigned int len)
{
	assert(data != NULL);

	m_slot2.writeModem(data, len);
}

unsigned int CDMRControl::readModemSlot1(unsigned char *data)
{
	assert(data != NULL);

	return m_slot1.readModem(data);
}

unsigned int CDMRControl::readModemSlot2(unsigned char *data)
{
	assert(data != NULL);

	return m_slot2.readModem(data);
}

void CDMRControl::clock()
{
	if (m_network != NULL) {
		CDMRData data;
		bool ret = m_network->read(data);
		if (ret) {
			unsigned int slotNo = data.getSlotNo();
			switch (slotNo) {
				case 1U: m_slot1.writeNetwork(data); break;
				case 2U: m_slot2.writeNetwork(data); break;
				default: LogError("Invalid slot no %u", slotNo); break;
			}
		}
	}

	m_slot1.clock();
	m_slot2.clock();
}
