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

#ifndef NFD_DAEMON_TABLE_SIT_HPP
#define NFD_DAEMON_TABLE_SIT_HPP

#include "name-tree.hpp"
#include "pit.hpp"
#include "sit-entry.hpp"

namespace nfd {
namespace pit {

/** \class DataMatchResult
 *  \brief an unordered iterable of all PIT entries matching Data
 *
 *  This type shall support:
 *    iterator<shared_ptr<pit::Entry>> begin()
 *    iterator<shared_ptr<pit::Entry>> end()
 */
typedef std::vector<shared_ptr<pit::SitEntry>> SitDataMatchResult;

} // namespace pit

/** \brief represents the Interest Table
 */
class Sit : public Pit
{
public:
  explicit
  Sit(NameTree& nameTree);

  ~Sit();

  /** \brief inserts a PIT entry for Interest
   *
   *  If an entry for exact same name and selectors exists, that entry is returned.
   *  \return the entry, and true for new entry, false for existing entry
   */
  std::pair<shared_ptr<pit::SitEntry>, bool>
  insert(const Interest& interest);

  /** \brief performs a Data match
   *  \return an iterable of all PIT entries matching data
   */
  pit::SitDataMatchResult
  findAllDataMatches(const Data& data) const;

  /**
   *  \brief erases a PIT Entry
   */
  void
  erase(shared_ptr<pit::SitEntry> pitEntry);

public: // enumeration
  class const_iterator;

  /** \brief returns an iterator pointing to the first PIT entry
   *  \note Iteration order is implementation-specific and is undefined
   *  \note The returned iterator may get invalidated if PIT or another NameTree-based
   *        table is modified
   */
  const_iterator
  begin() const;

  /** \brief returns an iterator referring to the past-the-end PIT entry
   *  \note The returned iterator may get invalidated if PIT or another NameTree-based
   *        table is modified
   */
  const_iterator
  end() const;

  class const_iterator : public std::iterator<std::forward_iterator_tag, const pit::SitEntry>
  {
  public:
    const_iterator();

    explicit
    const_iterator(const NameTree::const_iterator& it);

    ~const_iterator();

    const pit::SitEntry&
    operator*() const;

    shared_ptr<pit::SitEntry>
    operator->() const;

    const_iterator&
    operator++();

    const_iterator
    operator++(int);

    bool
    operator==(const const_iterator& other) const;

    bool
    operator!=(const const_iterator& other) const;

  private:
    NameTree::const_iterator m_nameTreeIterator;
    /** \brief Index of the current visiting PIT entry in NameTree node
     *
     * Index is used to ensure that dereferencing of m_nameTreeIterator happens only when
     * const_iterator is dereferenced or advanced.
     */
    size_t m_iPitEntry;
  };

};

inline
Sit::const_iterator::const_iterator()
  : m_iPitEntry(0)
{
}

inline
Sit::const_iterator::const_iterator(const NameTree::const_iterator& it)
  : m_nameTreeIterator(it)
  , m_iPitEntry(0)
{
}

inline
Sit::const_iterator::~const_iterator()
{
}

inline Sit::const_iterator
Sit::const_iterator::operator++(int)
{
  Sit::const_iterator temp(*this);
  ++(*this);
  return temp;
}

inline Sit::const_iterator&
Sit::const_iterator::operator++()
{
  ++m_iPitEntry;
  if (m_iPitEntry < m_nameTreeIterator->getSitEntries().size()) {
    return *this;
  }

  ++m_nameTreeIterator;
  m_iPitEntry = 0;
  return *this;
}

inline const pit::SitEntry&
Sit::const_iterator::operator*() const
{
  return *(this->operator->());
}

inline shared_ptr<pit::SitEntry>
Sit::const_iterator::operator->() const
{
  return m_nameTreeIterator->getSitEntries().at(m_iPitEntry);
}

inline bool
Sit::const_iterator::operator==(const Sit::const_iterator& other) const
{
  return m_nameTreeIterator == other.m_nameTreeIterator &&
         m_iPitEntry == other.m_iPitEntry;
}

inline bool
Sit::const_iterator::operator!=(const Sit::const_iterator& other) const
{
  return !(*this == other);
}

} // namespace nfd

#endif // NFD_DAEMON_TABLE_PIT_HPP
