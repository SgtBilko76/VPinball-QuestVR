#include "../common.h"

#include "RunningAnimationsCollection.h"

namespace B2SLegacy {

RunningAnimationsCollection* RunningAnimationsCollection::m_pInstance = nullptr;

RunningAnimationsCollection* RunningAnimationsCollection::GetInstance()
{
   if (!m_pInstance)
      m_pInstance = new RunningAnimationsCollection();

   return m_pInstance;
}

void RunningAnimationsCollection::Add(const string& item)
{
   if (std::ranges::find(*this, item) == end())
      push_back(item);
}

bool RunningAnimationsCollection::Remove(const string& item)
{
   if (auto it = std::ranges::find(*this, item); it != end())
      erase(it);

   return true;
}

bool RunningAnimationsCollection::Contains(const string& item) const
{
   return std::ranges::find(*this, item) != end();
}

}
