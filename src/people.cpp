/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file people.cpp People in the world. */

#include "stdafx.h"
#include "map.h"
#include "dates.h"
#include "math_func.h"
#include "geometry.h"
#include "person_type.h"
#include "ride_type.h"
#include "person.h"
#include "people.h"
#include "gamelevel.h"

Guests _guests; ///< %Guests in the world/park.
Staff _staff;   ///< %Staff in the world/park.

/**
 * Guest block constructor. Fills the id of the persons with an incrementing number.
 * @param base_id Id number of the first person in this block.
 */
GuestBlock::GuestBlock(uint16 base_id)
{
	for (uint i = 0; i < lengthof(this->guests); i++) {
		this->guests[i].id = base_id;
		base_id++;
	}
}

/**
 * Check that the voxel stack at the given coordinate is a good spot to use as entry point for new guests.
 * @param x X position at the edge.
 * @param y Y position at the edge.
 * @return The given position is a good starting point.
 */
static bool IsGoodEdgeRoad(int16 x, int16 y)
{
	if (x < 0 || y < 0) return false;
	int16 z = _world.GetBaseGroundHeight(x, y);
	const Voxel *vs = _world.GetVoxel(XYZPoint16(x, y, z));
	return vs && HasValidPath(vs) && GetImplodedPathSlope(vs) < PATH_FLAT_COUNT;
}

/**
 * Try to find a voxel at the edge of the world that can be used as entry point for guests.
 * @return The x/y coordinate at the edge of the world of a usable voxelstack, else an off-world coordinate.
 */
static Point16 FindEdgeRoad()
{
	int16 highest_x = _world.GetXSize() - 1;
	int16 highest_y = _world.GetYSize() - 1;
	for (int16 x = 1; x < highest_x; x++) {
		if (IsGoodEdgeRoad(x, 0))         return {x, 0};
		if (IsGoodEdgeRoad(x, highest_y)) return {x, highest_y};
	}
	for (int16 y = 1; y < highest_y; y++) {
		if (IsGoodEdgeRoad(0, y))         return {0, y};
		if (IsGoodEdgeRoad(highest_x, y)) return {highest_x, y};
	}

	return {-1, -1};
}

Guests::Guests() : block(0), rnd()
{
	this->free_idx = 0;
	this->start_voxel.x = -1;
	this->start_voxel.y = -1;
	this->daily_frac = 0;
	this->next_daily_index = 0;
}

Guests::~Guests()
{
}

/** Deactivate all guests and reset variables. */
void Guests::Uninitialize()
{
	for (int i = 0; i < GUEST_BLOCK_SIZE; i++) {
		Guest *g = this->block.Get(i);
		if (g->IsActive()) {
			g->DeActivate(OAR_REMOVE);
			this->AddFree(g);
		}
	}
	this->start_voxel.x = -1;
	this->start_voxel.y = -1;
	this->daily_frac = 0;
	this->next_daily_index = 0;
}

/**
 * Load guests from the save game.
 * @param ldr Input stream to read.
 */
void Guests::Load(Loader &ldr)
{
	uint32 version = ldr.OpenBlock("GSTS");
	if (version == 1) {
		this->start_voxel.x = ldr.GetWord();
		this->start_voxel.y = ldr.GetWord();
		this->daily_frac = ldr.GetWord();
		this->next_daily_index = ldr.GetWord();
		this->free_idx = ldr.GetLong();
		uint active_guest_count = ldr.GetLong();
		for (uint i = 0; i < active_guest_count; i++) {
			Guest *g = this->block.Get(ldr.GetWord());
			g->Load(ldr);
		}
	} else {
		ldr.SetFailMessage("Incorrect version of Guests block.");
	}
	ldr.CloseBlock();
}

/**
 * Save guests to the save game.
 * @param svr Output stream to save to.
 */
void Guests::Save(Saver &svr)
{
	svr.StartBlock("GSTS", 1);
	svr.PutWord(this->start_voxel.x);
	svr.PutWord(this->start_voxel.y);
	svr.PutWord(this->daily_frac);
	svr.PutWord(this->next_daily_index);
	svr.PutLong(this->free_idx);
	svr.PutLong(this->CountActiveGuests());
	for (uint i = 0; i < GUEST_BLOCK_SIZE; i++) {
		Guest *g = this->block.Get(i);
		if (g->IsActive()) {
			svr.PutWord(g->id);
			g->Save(svr);
		}
	}
	svr.EndBlock();
}

/**
 * Update #free_idx to the next free guest (if available).
 * @return Whether a free guest was found.
 */
bool Guests::FindNextFreeGuest()
{
	while (this->free_idx < GUEST_BLOCK_SIZE) {
		Guest *g = this->block.Get(this->free_idx);
		if (!g->IsActive()) return true;
		this->free_idx++;
	}
	return false;
}

/**
 * Update #free_idx to the next free guest (if available).
 * @return Whether a free guest was found.
 */
bool Guests::FindNextFreeGuest() const
{
	uint idx = this->free_idx;
	while (idx < GUEST_BLOCK_SIZE) {
		const Guest *g = this->block.Get(idx);
		if (!g->IsActive()) return true;
		idx++;
	}
	return false;
}

/**
 * Count the number of active guests.
 * @return The number of active guests.
 */
uint Guests::CountActiveGuests()
{
	uint count = this->free_idx;
	for (uint i = this->free_idx; i < GUEST_BLOCK_SIZE; i++) {
		Guest *g = this->block.Get(i);
		if (g->IsActive()) count++;
	}
	return count;
}

/**
 * Count the number of guests in the park.
 * @return The number of guests in the park.
 */
uint Guests::CountGuestsInPark()
{
	uint count = 0;
	for (uint i = 0; i < GUEST_BLOCK_SIZE; i++) {
		Guest *g = this->block.Get(i);
		if (g->IsActive() && g->IsInPark()) count++;
	}
	return count;
}

/**
 * Some time has passed, update the animation.
 * @param delay Number of milliseconds time that have past since the last animation update.
 */
void Guests::OnAnimate(int delay)
{
	for (int i = 0; i < GUEST_BLOCK_SIZE; i++) {
		Guest *p = this->block.Get(i);
		if (!p->IsActive()) continue;

		AnimateResult ar = p->OnAnimate(delay);
		if (ar != OAR_OK) {
			p->DeActivate(ar);
			this->AddFree(p);
		}
	}
}

/** A new frame arrived, perform the daily call for some of the guests. */
void Guests::DoTick()
{
	this->daily_frac++;
	int end_index = std::min(this->daily_frac * GUEST_BLOCK_SIZE / TICK_COUNT_PER_DAY, GUEST_BLOCK_SIZE);
	while (this->next_daily_index < end_index) {
		Guest *p = this->block.Get(this->next_daily_index);
		if (p->IsActive() && !p->DailyUpdate()) {
			p->DeActivate(OAR_REMOVE);
			this->AddFree(p);
		}
		this->next_daily_index++;
	}
	if (this->next_daily_index >= GUEST_BLOCK_SIZE) {
		this->daily_frac = 0;
		this->next_daily_index = 0;
	}
}

/**
 * A new day arrived, handle daily chores of the park.
 * @todo Add popularity rating concept.
 */
void Guests::OnNewDay()
{
	/* Try adding a new guest to the park. */
	if (this->CountActiveGuests() >= _scenario.max_guests) return;
	if (!this->rnd.Success1024(_scenario.GetSpawnProbability(512))) return;

	if (!IsGoodEdgeRoad(this->start_voxel.x, this->start_voxel.y)) {
		/* New guest, but no road. */
		this->start_voxel = FindEdgeRoad();
		if (!IsGoodEdgeRoad(this->start_voxel.x, this->start_voxel.y)) return;
	}

	if (!this->HasFreeGuests()) return; // No more quests available.
	/* New guest! */
	Guest *g = this->GetFree();
	g->Activate(this->start_voxel, PERSON_GUEST);
}

/**
 * Notification that the ride is being removed.
 * @param ri Ride being removed.
 */
void Guests::NotifyRideDeletion(const RideInstance *ri) {
	for (int i = 0; i < GUEST_BLOCK_SIZE; i++) {
		Guest *p = this->block.Get(i);
		if (!p->IsActive()) continue;

		p->NotifyRideDeletion(ri);
	}
}

/**
 * Return whether there are still non-active guests.
 * @return \c true if there are non-active guests, else \c false.
 */
bool Guests::HasFreeGuests() const
{
	return this->FindNextFreeGuest();
}

/**
 * Add a guest to the non-active list.
 * @param g %Guest to add.
 */
void Guests::AddFree(Guest *g)
{
	this->free_idx = std::min(this->block.Index(g), this->free_idx);
}

/**
 * Get a non-active guest.
 * @return A non-active guest.
 * @pre #HasFreeGuests() should hold.
 */
Guest *Guests::GetFree()
{
	bool b = this->FindNextFreeGuest();
	assert(b);

	Guest *g = this->block.Get(this->free_idx);
	this->free_idx++;
	return g;
}

Staff::Staff()
{
	/* Nothing to do currently. */
}

Staff::~Staff()
{
	/* Nothing to do currently. */
}

/** Remove all staff and reset all variables. */
void Staff::Uninitialize()
{
	this->mechanic_requests.clear();
}

/**
 * Load staff from the save game.
 * @param ldr Input stream to read.
 */
void Staff::Load(Loader &ldr)
{
	uint32 version = ldr.OpenBlock("STAF");
	if (version == 1) {
		for (uint i = ldr.GetLong(); i > 0; i--) {
			this->mechanic_requests.push_back(_rides_manager.GetRideInstance(ldr.GetWord()));
		}
	} else {
		ldr.SetFailMessage("Incorrect version of Staff block.");
	}
	ldr.CloseBlock();
}

/**
 * Save staff to the save game.
 * @param svr Output stream to save to.
 */
void Staff::Save(Saver &svr)
{
	svr.StartBlock("STAF", 1);
	svr.PutLong(this->mechanic_requests.size());
	for (RideInstance *ride : this->mechanic_requests) svr.PutWord(ride->GetIndex());
	svr.EndBlock();
}

/**
 * Request that a mechanic should inspect or repair a ride as soon as possible.
 * @param ride Ride to inspect or repair.
 */
void Staff::RequestMechanic(RideInstance *ride)
{
	mechanic_requests.push_back(ride);
}

/**
 * Notification that the ride is being removed.
 * @param ri Ride being removed.
 */
void Staff::NotifyRideDeletion(const RideInstance *ri) {
	/* \todo Forward this to the mechanics. */
}

/**
 * Some time has passed, update the animation.
 * @param delay Number of milliseconds time that have past since the last animation update.
 */
void Staff::OnAnimate(const int delay)
{
	/* \todo Forward this to the staff. */
}

/** A new frame arrived. */
void Staff::DoTick()
{
	/* \todo Assign mechanic requests to available mechanics. */
}

/** A new day arrived. */
void Staff::OnNewDay()
{
	/* Nothing to do currently. */
	/* But as long as mechanics are not implemented, we magically repair or inspect 1 ride per day. */
	if (!this->mechanic_requests.empty()) {
		Random rnd;
		if (rnd.Uniform(100) > 10) return;  // Don't do it at once though.
		this->mechanic_requests.front()->MechanicArrived();
		this->mechanic_requests.erase(this->mechanic_requests.begin());
	}
}
