/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
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

#include "sit-entry.hpp"
#include <algorithm>

namespace nfd {
namespace pit {

SitEntry::SitEntry(const Interest& interest)
  : Entry(interest)
{
}

SitInRecordCollection::iterator
SitEntry::insertOrUpdateInRecord(shared_ptr<Face> face, const Interest& interest)
{
  auto it = std::find_if(m_inRecords.begin(), m_inRecords.end(),
    [&face] (const InRecord& inRecord) { return inRecord.getFace() == face; });
  if (it == m_inRecords.end()) {
    m_inRecords.emplace_front(face);
    it = m_inRecords.begin();
  }

  it->update(interest);
  return it;
}

SitInRecordCollection::const_iterator
SitEntry::getInRecord(const Face& face) const
{
  return std::find_if(m_inRecords.begin(), m_inRecords.end(),
    [&face] (const InRecord& inRecord) { return inRecord.getFace().get() == &face; });
}

void
SitEntry::forwardInterest(shared_ptr<Face> face)
{       
  SitInRecordCollection::iterator it = std::find_if(m_inRecords.begin(), m_inRecords.end(),
    [&face] (const SitInRecord& inRecord) { return inRecord.getFace() == face; });
  if (it != m_inRecords.end()) {
    it->forward();
  }
}

time::steady_clock::TimePoint
SitEntry::getLastForwarded() const
{
  auto m = std::max_element(m_inRecords.begin(), m_inRecords.end(),
    [](const SitInRecord& a, const SitInRecord& b) { return a.getLastForwarded() < b.getLastForwarded(); }
  );
  return m->getLastForwarded();
}

} // namespace pit
} // namespace nfd
