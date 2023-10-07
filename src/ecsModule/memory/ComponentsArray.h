﻿#pragma once

#include <cassert>

#include "ComponentsArraySector.h"

namespace ECS::Memory {

	class ComponentsArray {
		ComponentsArray(const ComponentsArray&) = delete;
		ComponentsArray& operator=(ComponentsArray&) = delete;

	public:
		template<typename T>
		class Iterator {
		private:
			using VoidIt = SectorsChunk::Iterator;
			const uint16_t mOffset = 0;
			const uint8_t mTypePos = 0;
			VoidIt mIt;
		public:
			EntityId getSectorId() const { return *static_cast<EntityId*>(*mIt); }

			Iterator() {};
			Iterator(const VoidIt& listIt, uint16_t offset, uint8_t typePos) : mOffset(offset), mTypePos(typePos), mIt(listIt) {}

			T* operator*() { return static_cast<SectorInfo*>(*mIt)->isTypeNull(mTypePos) ? nullptr : static_cast<T*>(Utils::getTypePlace(*mIt, mOffset)); }
			Iterator& operator+(size_t i) {	mIt = mIt + i;	return *this; }
			Iterator& operator++() { ++mIt;	return *this; }

			bool operator!=(const Iterator& other) const { return mIt != other.mIt; }
		};

		template <typename T>
		Iterator<T> begin() {
			return { mChunk->begin(), mChunkData.sectorMembersOffsets[mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID]], mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID] };
		}

		template<typename T>
		Iterator<T> end() { return { mChunk->end(), 0, 0 }; }

		ComponentsArray(size_t capacity = 1);
		virtual ~ComponentsArray();

		size_t size() const;
		bool empty() const;
		void clear();
		void reserve(size_t newCapacity);
		void shrinkToFit();

		void* acquireSector(ECSType componentTypeId, EntityId entityId);
		void destroyObject(ECSType componentTypeId, EntityId entityId);
		void destroySector(EntityId entityId);

		template<typename T>
		T* getComponent(EntityId sectorId) {
			if (sectorId >= mSectorsMap.size() || mSectorsMap[sectorId] >= size()) {
				return nullptr;
			}

			const auto sectorPtr = (*mChunk)[mSectorsMap[sectorId]];
			if (static_cast<SectorInfo*>(sectorPtr)->isTypeNull(mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID])) {
				return nullptr;
			}

			return static_cast<T*>(Utils::getTypePlace(sectorPtr, T::STATIC_COMPONENT_TYPE_ID, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes));
		}

		template<typename T>
		void moveToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_TYPE_ID, sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			data->mOwnerId = sectorId;
			new (sector) T(std::move(*data));
		}

		template<typename T>
		void copyToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_TYPE_ID, sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			data->mOwnerId = sectorId;
			new (sector) T(*data);
		}

	private:
		void* initSectorMember(void* sectorPtr, ECSType componentTypeId);

		void setCapacity(size_t newCap);

		void* createSector(size_t pos, EntityId sectorId);
		void destroyObject(void* sectorPtr, ECSType componentTypeId);
		void destroySector(void* sectorPtr);

		void erase(size_t pos);

		std::vector<uint32_t> mSectorsMap;
		SectorsChunk* mChunk = nullptr;
		size_t mSize = 0;
		size_t mCapacity = 0;

	protected:
		void allocateChunk();
		ChunkData mChunkData;
	};

	template <typename... Types>
	class ComponentsArrayInitializer : public ComponentsArray {
		ComponentsArrayInitializer(const ComponentsArrayInitializer&) = delete;
		ComponentsArrayInitializer& operator=(ComponentsArrayInitializer&) = delete;

	public:
		ComponentsArrayInitializer(size_t capacity = 1) : ComponentsArray(capacity){
			static_assert(Utils::areUnique<Types...>(), "Duplicates detected in types");
			static_assert(sizeof...(Types) <= 32, "More then 32 components in one container not allowed");

			size_t idx = 2; //first is 0, 1 is for sector data
			((mChunkData.sectorMembersOffsets[idx++] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(Types)) + alignof(Types)), ...);
			

			uint8_t i = 0;
			((mChunkData.sectorMembersIndexes[Types::STATIC_COMPONENT_TYPE_ID] = ++i), ...);

			allocateChunk();
		}
	};
}
