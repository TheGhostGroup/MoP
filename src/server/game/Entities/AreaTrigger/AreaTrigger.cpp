/*
 * Copyright (C) 2012-2013 JadeCore <http://www.pandashan.com/>
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ObjectAccessor.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "Unit.h"
#include "SpellInfo.h"
#include "Log.h"
#include "AreaTrigger.h"
#include "ScriptMgr.h"

AreaTrigger::AreaTrigger() : WorldObject(false), _duration(0), m_caster(NULL), m_visualRadius(0.0f)
{
    m_objectType |= TYPEMASK_AREATRIGGER;
    m_objectTypeId = TYPEID_AREATRIGGER;

    m_updateFlag = UPDATEFLAG_STATIONARY_POSITION;

    m_valuesCount = AREATRIGGER_END;
}

AreaTrigger::~AreaTrigger()
{
    ASSERT(!m_caster);
}

void AreaTrigger::AddToWorld()
{
    ///- Register the AreaTrigger for guid lookup and for caster
    if (!IsInWorld())
    {
        sObjectAccessor->AddObject(this);
        WorldObject::AddToWorld();
        BindToCaster();
    }
}

void AreaTrigger::RemoveFromWorld()
{
    ///- Remove the AreaTrigger from the accessor and from all lists of objects in world
    if (IsInWorld())
    {
        UnbindFromCaster();
        WorldObject::RemoveFromWorld();
        sObjectAccessor->RemoveObject(this);
    }
}
bool AreaTrigger::CreateAreaTrigger(uint32 guidlow, uint32 triggerEntry, Unit* caster, SpellInfo const* spell, Position const& pos)
{
    SetMap(caster->GetMap());
    Relocate(pos);
    if (!IsPositionValid())
    {
        sLog->outError(LOG_FILTER_GENERAL, "AreaTrigger (spell %u) not created. Invalid coordinates (X: %f Y: %f)", spell->Id, GetPositionX(), GetPositionY());
        return false;
    }

    WorldObject::_Create(guidlow, HIGHGUID_AREATRIGGER, caster->GetPhaseMask());

    SetEntry(triggerEntry);
    SetDuration(spell->GetDuration());
    SetObjectScale(1);

    SetUInt64Value(AREATRIGGER_CASTER, caster->GetGUID());
    SetUInt32Value(AREATRIGGER_SPELLID, spell->Id);
    SetUInt32Value(AREATRIGGER_DURATION, spell->GetDuration());
    SetUInt32Value(AREATRIGGER_SPELLVISUALID, spell->SpellVisual[0]);

    switch (spell->Id)
    {
    case 116011:// Rune of Power
        SetVisualRadius(3.5f);
        break;
    case 116235:// Amethyst Pool
        SetVisualRadius(3.5f);
        break;
    case 123811:
        SetDuration(500000000);
        break;
    default:
        break;
    }

    if (!GetMap()->AddToMap(this))
        return false;

    return true;

}

void AreaTrigger::Update(uint32 p_time)
{
    if (GetDuration() > int32(p_time))
        _duration -= p_time;
    else
        Remove(); // expired

    WorldObject::Update(p_time);

    SpellInfo const* m_spellInfo = sSpellMgr->GetSpellInfo(GetUInt32Value(AREATRIGGER_SPELLID));
    if (!m_spellInfo)
        return;

    if (!GetCaster())
    {
        Remove();
        return;
    }

    Unit* caster = GetCaster();
    float radius = 0.0f;

    // Custom MoP Script
    switch (m_spellInfo->Id)
    {
    case 144479: //Expel Corruption
    {
        if (Player* nearest = this->FindNearestPlayer(1.0f, true))
        {
            this->Remove();
        }
        break;
    }
        // Drop Feathers.
    case 134338:
    {
        this->SetDuration(10000);

        if (Player* nearest = this->FindNearestPlayer(1.0f, true))
        {
            // Lesson of Icarus
            if (nearest->HasAura(140571))
                return;

            nearest->CastSpell(nearest, 140571);

            // Flight
            if (!nearest->HasSpell(133755))
                nearest->learnSpell(133755, true);

            nearest->GetSession()->SendNotification("You have just learnt [Flight] put it on your auction bar");

            // Daedling stack
            nearest->CastSpell(nearest, 140014);

            if (AuraPtr aura = nearest->GetAura(140014))
            {
                aura->SetStackAmount(4);
            }

            this->Remove();
        }
        break;
    }
        // defiled aura soo 2nd boss
    case 143960:
    {
        std::list<Player*> pl_list;
        pl_list.clear();

        this->GetPlayerListInGrid(pl_list, 2.0f);

        if (pl_list.empty())
            return;

        for (auto itr : pl_list)
        {
            itr->CastSpell(itr, 143959);

            if (AuraPtr aura = itr->GetAura(143959))
            {
                aura->SetDuration(1);
            }
        }
        break;
    }
    case 143235:
    {
        std::list<Player*> pl_list;
        pl_list.clear();

        this->GetPlayerListInGrid(pl_list, 2.0f);

        if (pl_list.empty())
            return;

        for (auto itr : pl_list)
        {
            itr->CastSpell(itr, 144367);
        }
        break;
    }
        // rushing waters
    case 147181:
    case 147178:
    case 147182:
    case 147191:
    case 147189:
    case 149164:
    {
        if (roll_chance_i(75))
        {
            Position pos;
            std::list<Unit*> targetList;
            radius = 1.0f;
            /*
            float y = 10.386f;
            float x = 0.512f;
            float newx, newy;

            newx = this->GetPositionX() - x;
            newy = this->GetPositionY() - y;
            pos.m_positionX = newx;
            pos.m_positionY = newy;
            */
            this->GetRandomNearPosition(pos, 100.0f);
            this->MovePosition(pos, 5.0F, this->GetOrientation());

            JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
            JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
            VisitNearbyObject(radius, searcher);

            for (auto itr : targetList)
            {
                itr->CastSpell(itr, 143412);

                if (AuraPtr aura = itr->GetAura(143412))
                {
                    aura->SetDuration(1);
                    this->Remove();
                }
            }

       
        }
        break;
    }
    case 13810: // Ice Trap
    {
        std::list<Unit*> targetList;
        radius = 10.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        for (auto itr : targetList)
            itr->CastSpell(itr, 135299, true);

        break;
    }
    case 102793:// Ursol's Vortex
    {
        std::list<Unit*> targetList;
        radius = 8.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
            for (auto itr : targetList)
                if (!itr->HasAura(127797))
                    caster->CastSpell(itr, 127797, true);

        break;
    }
    case 144692: // Pool of Fire Ordos
    {
        std::list<Player*> targetList;
        radius = 15.0f; // Not sure, needs check.

        GetPlayerListInGrid(targetList, 200.0f);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                // Pool of Fire - Periodic Damage
                if (itr->GetDistance(this) > radius)
                    itr->RemoveAurasDueToSpell(144693);
                else if (!itr->HasAura(144693))
                    caster->AddAura(144693, itr);
            }
        }
        break;
    }
    case 115460:// Healing Sphere
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                caster->CastSpell(itr, 115464, true); // Healing Sphere heal
                SetDuration(0);
                return;
            }
        }

        break;
    }
    case 115817:// Cancel Barrier
    {
        std::list<Unit*> targetList;
        radius = 6.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
            for (auto itr : targetList)
                itr->CastSpell(itr, 115856, true);

        break;
    }
    case 116011:// Rune of Power
    {
        std::list<Unit*> targetList;
        bool affected = false;
        radius = 2.25f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                if (itr->GetGUID() == caster->GetGUID())
                {
                    caster->CastSpell(itr, 116014, true); // Rune of Power
                    affected = true;

                    if (caster->ToPlayer())
                        caster->ToPlayer()->UpdateManaRegen();

                    return;
                }
            }
        }

        if (!affected)
            caster->RemoveAura(116014);

        break;
    }
    case 116235:// Amethyst Pool
    {
        std::list<Unit*> targetList;
        radius = 10.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                // Amethyst Pool - Periodic Damage
                if (itr->GetDistance(this) > 3.5f)
                    itr->RemoveAura(130774);
                else if (!itr->HasAura(130774))
                    caster->CastSpell(itr, 130774, true);
            }
        }

        break;
    }
    case 122731:// Create Cancelling Noise Area trigger
    {
        std::list<Unit*> targetList;
        radius = 10.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                // Periodic absorption for Imperial Vizier Zor'lok's Force and Verve and Sonic Rings
                if (itr->GetDistance(this) > 2.0f)
                    itr->RemoveAura(122706);
                else if (!itr->HasAura(122706))
                    caster->AddAura(122706, itr);
            }
        }
        break;
    }
    case 123461:// Get Away!
    {
        std::list<Player*> playerList;
        GetPlayerListInGrid(playerList, 60.0f);

        Position pos;
        GetPosition(&pos);

        for (auto player : playerList)
        {
            player->SendApplyMovementForce(true, pos, -3.0f);
        }

        break;
    }
    case 123811:// Pheromones of Zeal
    {
        std::list<Player*> targetList;
        radius = 35.0f;

        GetPlayerListInGrid(targetList, 200.0f);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                // Pheromones of Zeal - Periodic Damage
                if (itr->GetDistance(this) > radius)
                    itr->RemoveAura(123812);
                else if (!itr->HasAura(123812))
                    caster->AddAura(123812, itr);
            }
        }
        break;
    }
    case 116546:// Draw Power
    {
        std::list<Unit*> targetList;
        radius = 30.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        for (auto itr : targetList)
        {
            if (itr->IsInAxe(caster, this, 2.0f))
            {
                if (!itr->HasAura(116663))
                    caster->AddAura(116663, itr);
            }
            else
                itr->RemoveAurasDueToSpell(116663);
        }

        break;
    }
    case 117032:// Healing Sphere (Afterlife)
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                if (itr->GetGUID() == caster->GetGUID())
                {
                    caster->CastSpell(itr, 125355, true); // Heal for 15% of life
                    SetDuration(0);
                    return;
                }
            }
        }

        break;
    }
    case 119031:// Gift of the Serpent (Mastery)
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                caster->CastSpell(itr, 124041, true); // Gift of the Serpent heal
                SetDuration(0);
                return;
            }
        }

        break;
    }
    case 121286:// Chi Sphere (Afterlife)
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                if (itr->GetGUID() == caster->GetGUID())
                {
                    caster->CastSpell(itr, 121283, true); // Restore 1 Chi
                    SetDuration(0);
                    return;
                }
            }
        }

        break;
    }
    case 121536:// Angelic Feather
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                caster->CastSpell(itr, 121557, true); // Angelic Feather increase speed
                SetDuration(0);
                return;
            }
        }

        break;
    }
    case 122035:// Path of Blossom
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::NearestAttackableUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::NearestAttackableUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        if (!targetList.empty())
        {
            for (auto itr : targetList)
            {
                caster->CastSpell(itr, 122036, true); // Path of Blossom damage
                SetDuration(0);
                return;
            }
        }

        break;
    }
    case 124503:// Gift of the Ox
    case 124506:// Gift of the Ox?
    {
        std::list<Unit*> targetList;
        radius = 1.0f;

        JadeCore::AnyFriendlyUnitInObjectRangeCheck u_check(this, caster, radius);
        JadeCore::UnitListSearcher<JadeCore::AnyFriendlyUnitInObjectRangeCheck> searcher(this, targetList, u_check);
        VisitNearbyObject(radius, searcher);

        for (auto itr : targetList)
        {
            if (itr->GetGUID() != caster->GetGUID())
                continue;

            caster->CastSpell(itr, 124507, true); // Gift of the Ox - Heal
            SetDuration(0);
            return;
        }

        break;
    }
    default:
        break;
    }
}

void AreaTrigger::Remove()
{
    if (IsInWorld())
    {
        SpellInfo const* m_spellInfo = sSpellMgr->GetSpellInfo(GetUInt32Value(AREATRIGGER_SPELLID));
        if (!m_spellInfo)
            return;

        switch (m_spellInfo->Id)
        {
        case 116011:// Rune of Power : Remove the buff if caster is still in radius
            if (m_caster && m_caster->HasAura(116014))
                m_caster->RemoveAura(116014);
            break;
        case 122731:// Create Noise Cancelling Area Trigger
        {
            std::list<Player*> playerList;
            GetPlayerListInGrid(playerList, 200.0f);

            for (auto player : playerList)
                if (player->HasAura(122706))
                    player->RemoveAura(122706);
            break;
        }
        case 144692: // Pool of Fire Ordos
        {
            std::list<Player*> targetList;
            GetPlayerListInGrid(targetList, 100.0f);

            if (!targetList.empty())
                for (auto itr : targetList)
                    itr->RemoveAurasDueToSpell(144693); // Pool of Fire - Periodic Damage Remove.
            break;
        }
        case 123461:// Get Away!
        {
            std::list<Player*> playerList;
            GetPlayerListInGrid(playerList, 100.0f);

            Position pos;
            GetPosition(&pos);

            for (auto player : playerList)
                player->SendApplyMovementForce(false, pos);

            break;
        }
        default:
            break;
        }

        SendObjectDeSpawnAnim(GetGUID());
        RemoveFromWorld();
        AddObjectToRemoveList();
    }
}

void AreaTrigger::BindToCaster()
{
    //ASSERT(!m_caster);
    m_caster = ObjectAccessor::GetUnit(*this, GetCasterGUID());
    //ASSERT(GetCaster());
    //ASSERT(GetCaster()->GetMap() == GetMap());
    if (m_caster)
        m_caster->_RegisterAreaTrigger(this);
}

void AreaTrigger::UnbindFromCaster()
{
    ASSERT(m_caster);
    m_caster->_UnregisterAreaTrigger(this);
    m_caster = NULL;
}
