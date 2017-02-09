/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2016,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stateful-fw-strategy.hpp"
#include "algorithm.hpp"
#include "core/logger.hpp"
#include "ns3/ptr.h"
#include "ns3/node.h"
#include "model/ndn-net-device-transport.hpp"


#include "ns3/ndnSIM/helper/ndn-link-control-helper.hpp"
#include "ns3/assert.h"
#include "ns3/names.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/channel.h"
#include "ns3/log.h"
#include "ns3/error-model.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/pointer.h"

namespace nfd {
namespace fw {

NFD_LOG_INIT("StatefulForwardingStrategy");

const Name StatefulForwardingStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/stateful-fw/%FD%01");
NFD_REGISTER_STRATEGY(StatefulForwardingStrategy);

const time::milliseconds StatefulForwardingStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds StatefulForwardingStrategy::RETX_SUPPRESSION_MAX(250);

StatefulForwardingStrategy::StatefulForwardingStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
}

/** \brief determines whether a NextHop is eligible
 *  \param inFace incoming face of current Interest
 *  \param interest incoming Interest
 *  \param nexthop next hop
 *  \param pitEntry PIT entry
 *  \param wantUnused if true, NextHop must not have unexpired out-record
 *  \param now time::steady_clock::now(), ignored if !wantUnused
 */
static inline bool
isNextHopEligible(const Face& inFace, const Interest& interest,
                  const fib::NextHop& nexthop,
                  const shared_ptr<pit::Entry>& pitEntry,
                  bool wantUnused = false,
                  time::steady_clock::TimePoint now = time::steady_clock::TimePoint::min())
{
  const Face& outFace = nexthop.getFace();

  // do not forward back to the same face
  if (&outFace == &inFace)
    return false;

  // forwarding would violate scope
  if (wouldViolateScope(inFace, interest, outFace))
    return false;

  if (wantUnused) {
    // nexthop must not have unexpired out-record
    pit::OutRecordCollection::iterator outRecord = pitEntry->getOutRecord(outFace);
    if (outRecord != pitEntry->out_end() && outRecord->getExpiry() > now) {
      return false;
    }
  }

  return true;
}

/** \brief pick an eligible NextHop with earliest out-record
 *  \note It is assumed that every nexthop has an out-record.
 */
static inline fib::NextHopList::const_iterator
findEligibleNextHopWithEarliestOutRecord(const Face& inFace, const Interest& interest,
                                         const fib::NextHopList& nexthops,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
  fib::NextHopList::const_iterator found = nexthops.end();
  time::steady_clock::TimePoint earliestRenewed = time::steady_clock::TimePoint::max();
  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
    if (!isNextHopEligible(inFace, interest, *it, pitEntry))
      continue;
    pit::OutRecordCollection::iterator outRecord = pitEntry->getOutRecord(it->getFace());
    BOOST_ASSERT(outRecord != pitEntry->out_end());
    if (outRecord->getLastRenewed() < earliestRenewed) {
      found = it;
      earliestRenewed = outRecord->getLastRenewed();
    }
  }
  return found;
}

void
StatefulForwardingStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                         const shared_ptr<pit::Entry>& pitEntry)
{
std::cout << "Stateful FW strategy, afterReceiveInterest " << std::endl;

  RetxSuppression::Result suppression = m_retxSuppression.decide(inFace, interest, *pitEntry);
  if (suppression == RetxSuppression::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  fib::NextHopList::const_iterator it = nexthops.end();

  if (suppression == RetxSuppression::NEW) {
    // forward to nexthop with lowest cost except downstream
    it = std::find_if(nexthops.begin(), nexthops.end(),
      bind(&isNextHopEligible, cref(inFace), interest, _1, pitEntry,
           false, time::steady_clock::TimePoint::min()));

    if (it == nexthops.end()) {
      NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " noNextHop");

      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(pitEntry, inFace, nackHeader);

      this->rejectPendingInterest(pitEntry);
      return;
    }

    uint32_t inFaceNode1, inFaceNode2, outFaceNode1, outFaceNode2;

    // Get the 2 nodes associated with the incoming interface
    // This is compared with the 2 nodes associated with the outgoing interface, so that NACK can be sent from the correct node
    auto transportIn = dynamic_cast<ns3::ndn::NetDeviceTransport*>(inFace.getTransport()); //Get Transport for outFace
    if (transportIn != nullptr) {
        ns3::Ptr<ns3::PointToPointNetDevice> ndIn = transportIn->GetNetDevice()->GetObject<ns3::PointToPointNetDevice>();
        ns3::Ptr<ns3::Channel> channelIn = ndIn->GetChannel();
        ns3::Ptr<ns3::PointToPointChannel> ppChannelIn = ns3::DynamicCast<ns3::PointToPointChannel>(channelIn);
        ns3::Ptr<ns3::NetDevice> ndIn1 = ppChannelIn->GetDevice(0);
        ns3::Ptr<ns3::NetDevice> ndIn2 = ppChannelIn->GetDevice(1);

        inFaceNode1 = ndIn1->GetNode()->GetId();
	inFaceNode2 = ndIn2->GetNode()->GetId();
    }

    Face& outFace = it->getFace();

    // send NACK if the point-to-point link attached to the outFace is down

    // Get the 2 nodes associated with the outgoing interface
    auto transport = dynamic_cast<ns3::ndn::NetDeviceTransport*>(outFace.getTransport()); //Get Transport for outFace
    if (transport != nullptr) {
	ns3::Ptr<ns3::PointToPointNetDevice> nd = transport->GetNetDevice()->GetObject<ns3::PointToPointNetDevice>(); //Get point-to-point network device associated with the face
	ns3::Ptr<ns3::Channel> channel = nd->GetChannel(); //Get the channel associated with the network device
	ns3::Ptr<ns3::PointToPointChannel> ppChannel = ns3::DynamicCast<ns3::PointToPointChannel>(channel); //Get the channel type (point-to-point)
	ns3::Ptr<ns3::NetDevice> nd1 = ppChannel->GetDevice(0); //Get 1st network device associated with the point-to-point channel
	ns3::Ptr<ns3::NetDevice> nd2 = ppChannel->GetDevice(1); //Get 2nd network device associated with the point-to-point channel

        outFaceNode1 = nd1->GetNode()->GetId();
        outFaceNode2 = nd2->GetNode()->GetId();

        // Determine the node associated with the outFace to send the NACK from
	ns3::PointerValue pv;
        if (inFaceNode1 == outFaceNode1)
		nd1->GetAttribute("ReceiveErrorModel", pv);
        if (inFaceNode1 == outFaceNode2)
                nd2->GetAttribute("ReceiveErrorModel", pv);
	if (inFaceNode2 == outFaceNode1)
                nd1->GetAttribute("ReceiveErrorModel", pv);
        if (inFaceNode2 == outFaceNode2)
                nd2->GetAttribute("ReceiveErrorModel", pv);

	ns3::Ptr<ns3::Object> recvmodel = pv.GetObject ();
	if (recvmodel != nullptr) {
		ns3::Ptr<ns3::RateErrorModel> em = recvmodel->GetObject <ns3::RateErrorModel> ();
		NS_ASSERT (em != 0);
		ns3::BooleanValue devstate;
		em->GetAttribute ("IsEnabled", devstate);
		if (devstate == true) {
			// Send NACK to node from which interest was received
			lp::NackHeader nackHeader;
      			nackHeader.setReason(lp::NackReason::LINK_DOWN);
      			this->sendNack(pitEntry, inFace, nackHeader);
      			this->rejectPendingInterest(pitEntry);
      			return;
		}
		else {
			// Link is not down
		}
	}
    }

    //No NACK sent, so forward interest
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " newPitEntry-to=" << outFace.getId());
    return;
  }

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(),
                    bind(&isNextHopEligible, cref(inFace), interest, _1, pitEntry,
                         true, time::steady_clock::now()));
  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " retransmit-unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(inFace, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId() << " retransmitNoNextHop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << inFace.getId()
                           << " retransmit-retry-to=" << outFace.getId());
  }
}

/** \return less severe NackReason between x and y
 *
 *  lp::NackReason::NONE is treated as most severe
 */
inline lp::NackReason
compareLessSevere(lp::NackReason x, lp::NackReason y)
{
  if (x == lp::NackReason::NONE) {
    return y;
  }
  if (y == lp::NackReason::NONE) {
    return x;
  }
  return static_cast<lp::NackReason>(std::min(static_cast<int>(x), static_cast<int>(y)));
}

void
StatefulForwardingStrategy::afterReceiveNack(const Face& inFace, const lp::Nack& nack,
                                     const shared_ptr<pit::Entry>& pitEntry)
{

std::cout << "Stateful FW strategy, afterReceiveNack -- " << nack.getReason() << " -- RESEND " << nack.getInterest().getName() << std::endl;

  int nOutRecordsNotNacked = 0;
  Face* lastFaceNotNacked = nullptr;
  lp::NackReason leastSevereReason = lp::NackReason::NONE;
  for (const pit::OutRecord& outR : pitEntry->getOutRecords()) {
    const lp::NackHeader* inNack = outR.getIncomingNack();
    if (inNack == nullptr) {
      ++nOutRecordsNotNacked;
      lastFaceNotNacked = &outR.getFace();
      continue;
    }
    leastSevereReason = compareLessSevere(leastSevereReason, inNack->getReason());
  }

  lp::NackHeader outNack;
  outNack.setReason(leastSevereReason);

  if (nOutRecordsNotNacked == 1) {
    BOOST_ASSERT(lastFaceNotNacked != nullptr);
    pit::InRecordCollection::iterator inR = pitEntry->getInRecord(*lastFaceNotNacked);
    if (inR != pitEntry->in_end()) {
      // one out-record not Nacked, which is also a downstream
      NFD_LOG_DEBUG(nack.getInterest() << " nack-from=" << inFace.getId() <<
                    " nack=" << nack.getReason() <<
                    " nack-to(bidirectional)=" << lastFaceNotNacked->getId() <<
                    " out-nack=" << outNack.getReason());
      this->sendNack(pitEntry, *lastFaceNotNacked, outNack);
      return;
    }
  }

  if (nOutRecordsNotNacked > 0) {
    NFD_LOG_DEBUG(nack.getInterest() << " nack-from=" << inFace.getId() <<
                  " nack=" << nack.getReason() <<
                  " waiting=" << nOutRecordsNotNacked);
    // continue waiting
    return;
  }

  NFD_LOG_DEBUG(nack.getInterest() << " nack-from=" << inFace.getId() <<
                " nack=" << nack.getReason() <<
                " nack-to=all out-nack=" << outNack.getReason());

  //Resend interest upstream instead of sending NACK downstream
  Face& rtxFace = const_cast<Face&>(inFace);
  this->sendInterest(pitEntry, rtxFace, nack.getInterest());

//  this->sendNacks(pitEntry, outNack);
}

} // namespace fw
} // namespace nfd
