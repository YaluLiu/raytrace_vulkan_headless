#pragma once

#include <pxr/pxr.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

template <typename T, typename HandleT>
class SlotVector
{
public:
  HandleT Allocate(T value)
  {
    if(!_freeList.empty())
    {
      const uint32_t index = _freeList.back();
      _freeList.pop_back();
      Slot& slot = _slots[index];
      slot.value = std::move(value);
      slot.occupied = true;
      return HandleT{index, slot.generation};
    }

    Slot slot;
    slot.value = std::move(value);
    slot.occupied = true;
    _slots.emplace_back(std::move(slot));
    return HandleT{static_cast<uint32_t>(_slots.size() - 1), _slots.back().generation};
  }

  bool Free(HandleT handle)
  {
    Slot* slot = _GetSlot(handle);
    if(slot == nullptr)
    {
      return false;
    }

    slot->occupied = false;
    ++slot->generation;
    _freeList.push_back(handle.index);
    return true;
  }

  bool Retire(HandleT handle)
  {
    Slot* slot = _GetSlot(handle);
    if(slot == nullptr)
    {
      return false;
    }

    ++slot->generation;
    return true;
  }

  T* Get(HandleT handle)
  {
    Slot* slot = _GetSlot(handle);
    return slot == nullptr ? nullptr : &slot->value;
  }

  const T* Get(HandleT handle) const
  {
    const Slot* slot = _GetSlot(handle);
    return slot == nullptr ? nullptr : &slot->value;
  }

  T* GetByIndex(uint32_t index)
  {
    if(index >= _slots.size() || !_slots[index].occupied)
    {
      return nullptr;
    }
    return &_slots[index].value;
  }

  const T* GetByIndex(uint32_t index) const
  {
    if(index >= _slots.size() || !_slots[index].occupied)
    {
      return nullptr;
    }
    return &_slots[index].value;
  }

  std::vector<T> GetSnapshot() const
  {
    std::vector<T> snapshot;
    snapshot.reserve(_slots.size());
    for(const Slot& slot : _slots)
    {
      if(slot.occupied)
      {
        snapshot.push_back(slot.value);
      }
    }
    return snapshot;
  }

  size_t Size() const { return _slots.size(); }

private:
  struct Slot
  {
    T value;
    uint32_t generation = 1;
    bool occupied = false;
  };

  Slot* _GetSlot(HandleT handle)
  {
    if(handle.index >= _slots.size())
    {
      return nullptr;
    }

    Slot& slot = _slots[handle.index];
    if(!slot.occupied || slot.generation != handle.generation)
    {
      return nullptr;
    }
    return &slot;
  }

  const Slot* _GetSlot(HandleT handle) const
  {
    if(handle.index >= _slots.size())
    {
      return nullptr;
    }

    const Slot& slot = _slots[handle.index];
    if(!slot.occupied || slot.generation != handle.generation)
    {
      return nullptr;
    }
    return &slot;
  }

  std::vector<Slot> _slots;
  std::vector<uint32_t> _freeList;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
