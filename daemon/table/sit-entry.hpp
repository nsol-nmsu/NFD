/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
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

#ifndef NFD_DAEMON_TABLE_SIT_ENTRY_HPP
#define NFD_DAEMON_TABLE_SIT_ENTRY_HPP

#include "pit-entry.hpp"
#include "sit-in-record.hpp"
#include "core/scheduler.hpp"

namespace nfd {

namespace pit {

/** \brief represents an unordered collection of InRecords
 */
typedef std::list< SitInRecord>  SitInRecordCollection;

/** \brief represents a PIT entry
 */
class SitEntry : public Entry
{
public:
  explicit
  SitEntry(const Interest& interest);
  
  void
  forwardInterest(shared_ptr<Face> face);
  
  time::steady_clock::TimePoint
  getLastForwarded() const;

public: // InRecord
  const SitInRecordCollection&
  getInRecords() const;

  /** \brief inserts a InRecord for face, and updates it with interest
   *
   *  If InRecord for face exists, the existing one is updated.
   *  This method does not add the Nonce as a seen Nonce.
   *  \return an iterator to the InRecord
   */
  SitInRecordCollection::iterator
  insertOrUpdateInRecord(shared_ptr<Face> face, const Interest& interest);

  /** \brief get the InRecord for face
   *  \return an iterator to the InRecord, or .end if it does not exist
   */
  SitInRecordCollection::const_iterator
  getInRecord(const Face& face) const;

protected:
  SitInRecordCollection m_inRecords;
  
};

inline const SitInRecordCollection&
SitEntry::getInRecords() const
{
  return m_inRecords;
}

} // namespace pit
} // namespace nfd

#endif // NFD_DAEMON_TABLE_PIT_ENTRY_HPP
