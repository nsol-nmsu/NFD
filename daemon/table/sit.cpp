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

#include "sit.hpp"
#include "sit-entry.hpp"
#include <type_traits>

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <type_traits>

namespace nfd {
namespace pit {

#if HAVE_IS_MOVE_CONSTRUCTIBLE
static_assert(std::is_move_constructible<SitDataMatchResult>::value,
              "SitDataMatchResult must be MoveConstructible");
#endif // HAVE_IS_MOVE_CONSTRUCTIBLE

}

// http://en.cppreference.com/w/cpp/concept/ForwardIterator
BOOST_CONCEPT_ASSERT((boost::ForwardIterator<Sit::const_iterator>));
// boost::ForwardIterator follows SGI standard http://www.sgi.com/tech/stl/ForwardIterator.html,
// which doesn't require DefaultConstructible
#ifdef HAVE_IS_DEFAULT_CONSTRUCTIBLE
static_assert(std::is_default_constructible<Sit::const_iterator>::value,
              "Sit::const_iterator must be default-constructible");
#else
BOOST_CONCEPT_ASSERT((boost::DefaultConstructible<Sit::const_iterator>));
#endif // HAVE_IS_DEFAULT_CONSTRUCTIBLE

Sit::Sit(NameTree& nameTree)
  : Pit(nameTree)
{
}

Sit::~Sit()
{
}

std::pair<shared_ptr<pit::SitEntry>, bool>
Sit::insert(const Interest& interest)
{
  // first lookup() the Interest Name in the NameTree, which will creates all
  // the intermedia nodes, starting from the shortest prefix.
  shared_ptr<name_tree::Entry> nameTreeEntry = m_nameTree.lookup(interest.getName());
  BOOST_ASSERT(static_cast<bool>(nameTreeEntry));

  const std::vector<shared_ptr<pit::SitEntry>>& pitEntries = nameTreeEntry->getSitEntries();

  // then check if this Interest is already in the PIT entries
  auto it = std::find_if(pitEntries.begin(), pitEntries.end(),
                         [&interest] (const shared_ptr<pit::SitEntry>& entry) {
                                return entry->getInterest().getName() == interest.getName() &&
                                       entry->getInterest().getSelectors() == interest.getSelectors();
                         });
  if (it != pitEntries.end()) {
    return { *it, false };
  }

  shared_ptr<pit::SitEntry> entry = make_shared<pit::SitEntry>(interest);
  nameTreeEntry->insertSitEntry(entry);
  m_nItems++;
  return { entry, true };
}

pit::SitDataMatchResult
Sit::findAllDataMatches(const Data& data) const
{
  auto&& ntMatches = m_nameTree.findAllMatches(data.getName(),
    [] (const name_tree::Entry& entry) { return entry.hasSitEntries(); });

  pit::SitDataMatchResult matches;
  for (const name_tree::Entry& nte : ntMatches) {
    for (const shared_ptr<pit::SitEntry>& pitEntry : nte.getSitEntries()) {
      if (pitEntry->getInterest().matchesData(data))
        matches.emplace_back(pitEntry);
    }
  }

  return matches;
}

void
Sit::erase(shared_ptr<pit::SitEntry> pitEntry)
{
  shared_ptr<name_tree::Entry> nameTreeEntry = m_nameTree.get(*pitEntry);
  BOOST_ASSERT(static_cast<bool>(nameTreeEntry));

  nameTreeEntry->eraseSitEntry(pitEntry);
  m_nameTree.eraseEntryIfEmpty(nameTreeEntry);

  --m_nItems;
}

Sit::const_iterator
Sit::begin() const
{
  return const_iterator(m_nameTree.fullEnumerate(
    [] (const name_tree::Entry& entry) { return entry.hasSitEntries(); }).begin());
}

} // namespace nfd
