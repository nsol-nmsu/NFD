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

/*
#include "sit-entry.hpp"
#include <type_traits>

#include <boost/concept/assert.hpp>
#include <boost/concept_check.hpp>
#include <type_traits>
*/
namespace nfd {
namespace pit {

static inline bool
nteHasSitEntries(const name_tree::Entry& nte)
{
  return nte.hasSitEntries();
}

Sit::Sit(NameTree& nameTree) 
  : Pit(nameTree)

{
}

Sit::~Sit()
{
}

std::pair<shared_ptr<pit::SitEntry>, bool>
Sit::findOrInsert(const Interest& interest, bool allowInsert)
{
   // determine which NameTree entry should the SIT entry be attached onto
   const Name& name = interest.getName();
   bool isEndWithDigest = name.size() > 0 && name[-1].isImplicitSha256Digest();
   const Name& nteName = isEndWithDigest ? name.getPrefix(-1) : name;

   // ensure NameTree entry exists
   name_tree::Entry* nte = nullptr;
   if (allowInsert) {
     nte = &m_nameTree.lookup(nteName);
   }
   else {
     nte = m_nameTree.findExactMatch(nteName);
     if (nte == nullptr) {
        return {nullptr, true};
     }
   }

   // check if SIT entry already exists
   size_t nteNameLen = nteName.size();
   const std::vector<shared_ptr<pit::SitEntry>>& sitEntries = nte->getSitEntries();
   auto it = std::find_if(sitEntries.begin(), sitEntries.end(),
    [&interest, nteNameLen] (const shared_ptr<pit::SitEntry>& entry) {
       // initial part of name is guaranteed to be equal by NameTree
       // check implicit digest (or its absence) only
       return entry->canMatch(interest, nteNameLen);
    });
   if (it != sitEntries.end()) {
      return {*it, false};
   }

   if (!allowInsert) {
      BOOST_ASSERT(!nte->isEmpty()); // nte shouldn't be created in this call
      return {nullptr, true};
   }

   auto entry = make_shared<pit::SitEntry>(interest);
   nte->insertSitEntry(entry);
   ++m_nItems;
   return {entry, true};
}

SitDataMatchResult
Sit::findAllDataMatches(const Data& data) const
{

  auto&& ntMatches = m_nameTree.findAllMatches(data.getName(), &nteHasSitEntries);

  SitDataMatchResult matches;
  for (const name_tree::Entry& nte : ntMatches) {
     for (const shared_ptr<pit::SitEntry>& sitEntry : nte.getSitEntries()) {
        if (sitEntry->getInterest().matchesData(data))
           matches.emplace_back(sitEntry);
     }
  }

  return matches;
}


void
Sit::erase(pit::SitEntry* entry, bool canDeleteNte)
{
   name_tree::Entry* nte = m_nameTree.getEntry(*entry);
   BOOST_ASSERT(nte != nullptr);

   nte->eraseSitEntry(entry);
   if (canDeleteNte) {
     m_nameTree.eraseIfEmpty(nte);
   }
   --m_nItems;
}

void
Sit::deleteInOutRecords(pit::SitEntry* entry, const Face& face)
{
  BOOST_ASSERT(entry != nullptr);

  entry->deleteInRecord(face);
  entry->deleteOutRecord(face);

}

Sit::const_iterator
Sit::begin() const
{
  return const_iterator(m_nameTree.fullEnumerate(&nteHasSitEntries).begin());
}

} // namespace pit
} // namespace nfd
