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
#include "sit-out-record.hpp"
#include "core/scheduler.hpp"

namespace nfd {

namespace name_tree {
class Entry;
} // namespace name_tree

namespace pit {

/** \brief represents an unordered collection of InRecords
 */
typedef std::list< SitInRecord>  SitInRecordCollection;

typedef std::list< SitOutRecord>  SitOutRecordCollection;

/** \brief represents a PIT entry
 */
class SitEntry : public Entry //public StrategyInfoHost, noncopyable
{
public:
  explicit
  SitEntry(const Interest& interest);

  const Interest&
  getInterest() const
  {
     return *m_interest;
  }

  const Name&
  getName() const
  {
     return m_interest->getName();
  }

  bool
  canMatch(const Interest& interest, size_t nEqualNameComps = 0) const;

  public: // in-record
    const SitInRecordCollection&
    getInRecords() const
    {
      return m_inRecords;
    }

    bool
    hasInRecords() const
    {
      return !m_inRecords.empty();
    }

    SitInRecordCollection::iterator
    in_begin()
    {
      return m_inRecords.begin();
    }

    SitInRecordCollection::const_iterator
    in_begin() const
    {
      return m_inRecords.begin();
    }

    SitInRecordCollection::iterator
    in_end()
    {
      return m_inRecords.end();
    }

    SitInRecordCollection::const_iterator
    in_end() const
    {
      return m_inRecords.end();
    }

    SitInRecordCollection::iterator
    getInRecord(const Face& face);

    SitInRecordCollection::iterator
    insertOrUpdateInRecord(Face& face, const Interest& interest);

    void
    deleteInRecord(const Face& face);

    void
    clearInRecords();

  public: // out-record
    const SitOutRecordCollection&
    getOutRecords() const
    {
      return m_outRecords;
    }

    bool
    hasOutRecords() const
    {
       return !m_outRecords.empty();
    }

    SitOutRecordCollection::iterator
    out_begin()
    {
      return m_outRecords.begin();
    }

    SitOutRecordCollection::const_iterator
    out_begin() const
    {
      return m_outRecords.begin();
    }

    SitOutRecordCollection::iterator
    out_end()
    {
      return m_outRecords.end();
    }

    SitOutRecordCollection::const_iterator
    out_end() const
    {
      return m_outRecords.end();
    }

    SitOutRecordCollection::iterator
    getOutRecord(const Face& face);

    SitOutRecordCollection::iterator
    insertOrUpdateOutRecord(Face& face, const Interest& interest);

    void
    deleteOutRecord(const Face& face);

 public:
    scheduler::EventId m_unsatisfyTimer;

   scheduler::EventId m_stragglerTimer;

 private:
    SitInRecordCollection m_inRecords;
    SitOutRecordCollection m_outRecords;

//    shared_ptr<const Interest> m_interest;
  //  name_tree::Entry* m_nameTreeEntry;

    friend class name_tree::Entry;

};

} // namespace pit
} // namespace nfd

#endif // NFD_DAEMON_TABLE_PIT_ENTRY_HPP
