/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
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

/*
 * Scripts for spells with MASTERY.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_mastery_".
 */

#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"

enum MasterySpells
{
    MASTERY_SPELL_LIGHTNING_BOLT        = 45284,
    MASTERY_SPELL_CHAIN_LIGHTNING       = 45297,
    MASTERY_SPELL_LAVA_BURST            = 77451,
    MASTERY_SPELL_ELEMENTAL_BLAST       = 120588,
    MASTERY_SPELL_HAND_OF_LIGHT         = 96172,
    MASTERY_SPELL_IGNITE                = 12654,
    MASTERY_SPELL_BLOOD_SHIELD          = 77535,
    MASTERY_SPELL_DISCIPLINE_SHIELD     = 77484,
    SPELL_DK_SCENT_OF_BLOOD             = 50421,
    SPELL_MAGE_MASTERY_ICICLES          = 76613,
    SPELL_MAGE_ICICLE_DAMAGE            = 148022,
    SPELL_MAGE_FINGERS_OF_FROST         = 44544,
    SPELL_MAGE_ICICLE_PERIODIC_TRIGGER  = 148023,
    SPELL_PRIEST_ECHO_OF_LIGHT          = 77489
};

// Called by Power Word : Shield - 17, Power Word : Shield (Divine Insight) - 123258, Spirit Shell - 114908, Angelic Bulwark - 114214 and Divine Aegis - 47753
// Mastery : Shield Discipline - 77484
class spell_mastery_shield_discipline : public SpellScriptLoader
{
    public:
        spell_mastery_shield_discipline() : SpellScriptLoader("spell_mastery_shield_discipline") { }

        class spell_mastery_shield_discipline_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mastery_shield_discipline_AuraScript);

            void CalculateAmount(constAuraEffectPtr , int32 & amount, bool & )
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->HasAura(MASTERY_SPELL_DISCIPLINE_SHIELD) && caster->getLevel() >= 80)
                    {
                        float Mastery = 1 + (caster->GetFloatValue(PLAYER_MASTERY) * 1.6f / 100.0f);
                        amount = int32(amount * Mastery);
                    }
                }
            }

            void Register()
            {
                DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_mastery_shield_discipline_AuraScript::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mastery_shield_discipline_AuraScript();
        }
};

// Called by 45470 - Death Strike (Heal)
// 77513 - Mastery : Blood Shield
class spell_mastery_blood_shield : public SpellScriptLoader
{
    public:
        spell_mastery_blood_shield() : SpellScriptLoader("spell_mastery_blood_shield") { }

        class spell_mastery_blood_shield_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_blood_shield_SpellScript);

            void HandleAfterHit()
            {
                if (Player* _plr = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (_plr->GetTypeId() == TYPEID_PLAYER && _plr->HasAura(77513) && _plr->getLevel() >= 80)
                        {
                            // Check the Mastery aura while in Blood presence
                            if (_plr->HasAura(77513) && _plr->HasAura(48263))
                            {
                                float Mastery = _plr->GetFloatValue(PLAYER_MASTERY) * 6.25f / 100.0f;

                                int32 bp = -int32(GetHitDamage() * Mastery);

                                if (AuraPtr scentOfBlood = _plr->GetAura(SPELL_DK_SCENT_OF_BLOOD))
                                    AddPct(bp, (scentOfBlood->GetStackAmount() * 20));

                                if (AuraEffectPtr bloodShield = target->GetAuraEffect(MASTERY_SPELL_BLOOD_SHIELD, EFFECT_0))
                                    bp += bloodShield->GetAmount();

                                bp = std::min(uint32(bp), target->GetMaxHealth());

                                _plr->CastCustomSpell(target, MASTERY_SPELL_BLOOD_SHIELD, &bp, NULL, NULL, true);
                            }
                        }
                    }
                }
            }

            void Register()
            {
                AfterHit += SpellHitFn(spell_mastery_blood_shield_SpellScript::HandleAfterHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_blood_shield_SpellScript();
        }
};

// Called by 133 - Fireball, 44614 - Frostfire Bolt, 108853 - Inferno Blast, 2948 - Scorch and 11366 - Pyroblast
// 12846 - Mastery : Ignite
class spell_mastery_ignite : public SpellScriptLoader
{
    public:
        spell_mastery_ignite() : SpellScriptLoader("spell_mastery_ignite") { }

        class spell_mastery_ignite_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_ignite_SpellScript);

            void HandleAfterHit()
            {
                if (Unit* caster = GetCaster())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (caster->GetTypeId() == TYPEID_PLAYER && caster->HasAura(12846) && caster->getLevel() >= 80)
                        {
                            uint32 procSpellId = GetSpellInfo()->Id ? GetSpellInfo()->Id : 0;
                            if (procSpellId != MASTERY_SPELL_IGNITE)
                            {
                                float value = caster->GetFloatValue(PLAYER_MASTERY) * 1.5f / 100.0f;

                                int32 bp = GetHitDamage();
                                bp = int32(bp * value / 2);

                                if (target->HasAura(MASTERY_SPELL_IGNITE, caster->GetGUID()))
                                {
                                    bp += target->GetRemainingPeriodicAmount(caster->GetGUID(), MASTERY_SPELL_IGNITE, SPELL_AURA_PERIODIC_DAMAGE);
                                    bp = int32(bp * 0.66f);
                                }

                                caster->CastCustomSpell(target, MASTERY_SPELL_IGNITE, &bp, NULL, NULL, true);
                            }
                        }
                    }
                }
            }

            void Register()
            {
                AfterHit += SpellHitFn(spell_mastery_ignite_SpellScript::HandleAfterHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_ignite_SpellScript();
        }
};

// Called by 35395 - Crusader Strike, 53595 - Hammer of the Righteous, 24275 - Hammer of Wrath, 85256 - Templar's Verdict and 53385 - Divine Storm
// 76672 - Mastery : Hand of Light
class spell_mastery_hand_of_light : public SpellScriptLoader
{
    public:
        spell_mastery_hand_of_light() : SpellScriptLoader("spell_mastery_hand_of_light") { }

        class spell_mastery_hand_of_light_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_hand_of_light_SpellScript);

            void HandleAfterHit()
            {
                if (Unit* caster = GetCaster())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (caster->GetTypeId() == TYPEID_PLAYER && caster->GetGUID() != target->GetGUID()&& caster->HasAura(76672) && caster->getLevel() >= 80)
                        {
                            uint32 procSpellId = GetSpellInfo()->Id ? GetSpellInfo()->Id : 0;
                            if (procSpellId != MASTERY_SPELL_HAND_OF_LIGHT)
                            {
                                float value = caster->GetFloatValue(PLAYER_MASTERY) * 1.85f;

                                int32 bp = int32(GetHitDamage() * value / 100);

                                // [Inquisition - 84963] does increase the holy damage done by Mastery : Hand of Light - 76672
                                if (caster->HasAura(84963))
                                    bp *= 1.3f;

                                // Need to recalculate if damage is absorbed
                                if (bp == 0)
                                {
                                    int32 absorbedDamage = int32(GetAbsorbedDamage() * value / 100);
                                    // If spell didn't hit, absorbedDamage will be random negative value, because of it players receive 1kk+ heal/damage from it.
                                    // 100000 - just in case, to prevent bug with high damage, because it's unreal to deal ~666k damage (666k*0.15%=100k).
                                    if (absorbedDamage > 0 && absorbedDamage < 100000)
                                        caster->CastCustomSpell(target, MASTERY_SPELL_HAND_OF_LIGHT, &absorbedDamage, NULL, NULL, true);
                                }
                                else
                                    caster->CastCustomSpell(target, MASTERY_SPELL_HAND_OF_LIGHT, &bp, NULL, NULL, true);
                            }
                        }
                    }
                }
            }

            void Register()
            {
                AfterHit += SpellHitFn(spell_mastery_hand_of_light_SpellScript::HandleAfterHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_hand_of_light_SpellScript();
        }
};

const int IcicleAuras[5] = { 148012, 148013, 148014, 148015, 148016 };
const int IcicleHits[5] = { 148017, 148018, 148019, 148020, 148021 };
bool IcicleOverstack = false;

// Called by Frostbolt - 116 and Frostfire bolt - 44614
// Mastery: Icicles - 76613
class spell_mastery_icicles : public SpellScriptLoader
{
    public:
        spell_mastery_icicles() : SpellScriptLoader("spell_mastery_icicles") { }

        class spell_mastery_icicles_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_icicles_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        // Calculate damage
                        int32 hitDamage = GetHitDamage();
                        if (GetSpell()->IsCritForTarget(target))
                            hitDamage *= 2;
                        float Mastery = (_player->GetFloatValue(PLAYER_MASTERY) * 2.0f) / 100.0f;
                        hitDamage *= Mastery;

                        int8 currentMaxAura = 0;

                        // if hitDamage == 0 we have a miss, so we need to except this variant
                        if (_player->HasAura(SPELL_MAGE_MASTERY_ICICLES) && hitDamage != 0)
                        {
                            // We need to find how much icicles we have, and which is the last.
                            for (int i = 0; i < 5; i++)
                                if (_player->HasAura(IcicleAuras[i]))
                                    currentMaxAura = i+1;

                            switch (currentMaxAura)
                            {
                                case 0:
                                    _player->AddAura(IcicleAuras[0], _player);
                                    break;
                                case 1:
                                    _player->AddAura(IcicleAuras[1], _player);
                                    break;
                                case 2:
                                    _player->AddAura(IcicleAuras[2], _player);
                                    break;
                                case 3:
                                    _player->AddAura(IcicleAuras[3], _player);
                                    break;
                                case 4:
                                    _player->AddAura(IcicleAuras[4], _player);
                                    break;
                                case 5:
                                {
                                    // We need to find an icicle, with the smallest duration.
                                    int32 minDuration = 0;
                                    int32 smallestIcicle = 0;
                                    if (AuraPtr icicleCurrentAura = _player->GetAura(IcicleAuras[smallestIcicle]))
                                        minDuration = _player->GetAura(IcicleAuras[0])->GetDuration();

                                    for (int i = 1; i < 5; i++)
                                    {
                                        if (AuraPtr tmpCurrentAura = _player->GetAura(IcicleAuras[i]))
                                        {
                                            if (minDuration > tmpCurrentAura->GetDuration())
                                            {
                                                minDuration = tmpCurrentAura->GetDuration();
                                                smallestIcicle = i;
                                            }
                                        }
                                    }

                                    if (AuraPtr icicleCurrentAura = _player->GetAura(IcicleAuras[smallestIcicle]))
                                    {
                                        int32 basepoints = icicleCurrentAura->GetEffect(0)->GetAmount();
                                        _player->CastSpell(target, IcicleHits[smallestIcicle], true);
                                        _player->CastCustomSpell(target, SPELL_MAGE_ICICLE_DAMAGE, &basepoints, NULL, NULL, true);
                                        _player->RemoveAura(IcicleAuras[smallestIcicle]);
                                    }

                                    _player->AddAura(IcicleAuras[smallestIcicle], _player);

                                    if (AuraPtr icicleCurrentAura = _player->GetAura(IcicleAuras[smallestIcicle]))
                                        icicleCurrentAura->GetEffect(0)->SetAmount(int32(hitDamage));

                                    IcicleOverstack = true;
                                    break;
                                }
                            }

                            if (IcicleOverstack == false)
                                if (AuraPtr icicleCurrentAura = _player->GetAura(IcicleAuras[currentMaxAura]))
                                    icicleCurrentAura->GetEffect(0)->SetAmount(hitDamage);
                        }
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mastery_icicles_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_icicles_SpellScript();
        }
};

// Ice Lance - 30455
class spell_mastery_icicles_trigger : public SpellScriptLoader
{
    public:
        spell_mastery_icicles_trigger() : SpellScriptLoader("spell_mastery_icicles_trigger") { }

        class spell_mastery_icicles_trigger_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_icicles_trigger_SpellScript);

            void HandleOnHit(SpellEffIndex effIndex)
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->HasAura(44544))
                        SetHitDamage(int32(GetHitDamage() * 1.25f));

                    if (GetHitUnit())
                        caster->SetIciclesTarget(GetHitUnit()->GetGUID());

                    caster->CastSpell(caster, SPELL_MAGE_ICICLE_PERIODIC_TRIGGER, true);
                }
            }

            void Register()
            {
                OnEffectHitTarget += SpellEffectFn(spell_mastery_icicles_trigger_SpellScript::HandleOnHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_icicles_trigger_SpellScript();
        }
};

// Icicles (periodic) - 148023
class spell_mastery_icicles_periodic : public SpellScriptLoader
{
    public:
        spell_mastery_icicles_periodic() : SpellScriptLoader("spell_mastery_icicles_periodic") { }

        class spell_mastery_icicles_periodic_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mastery_icicles_periodic_AuraScript);

            void OnTick(constAuraEffectPtr aurEff)
            {
                if (Unit* caster = GetCaster())
                {
                    if (AuraEffectPtr aura = caster->GetAuraEffect(GetSpellInfo()->Id, EFFECT_0))
                    {
                        if (aura->GetAmount() > 4)
                            caster->RemoveAura(GetSpellInfo()->Id);

                        // Maybe not the good target selection ...
                        if (Unit* target = ObjectAccessor::FindUnit(caster->GetIciclesTarget()))
                        {
                            if (AuraPtr icicleCurrentAura = caster->GetAura(IcicleAuras[aura->GetAmount()]))
                            {
                                int32 basepoints = icicleCurrentAura->GetEffect(0)->GetAmount();
                                caster->CastSpell(target, IcicleHits[aura->GetAmount()], true);
                                caster->CastCustomSpell(target, SPELL_MAGE_ICICLE_DAMAGE, &basepoints, NULL, NULL, true);
                                caster->RemoveAura(IcicleAuras[aura->GetAmount()]);
                                aura->SetAmount(aura->GetAmount() + 1);
                            }

                            IcicleOverstack = false;
                        }
                    }
                }
            }

            void Register()
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_mastery_icicles_periodic_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mastery_icicles_periodic_AuraScript();
        }
};

// Icicles: 148017, 148018, 148019, 148020, 148021
class spell_mastery_icicles_hit : public SpellScriptLoader
{
    public:
        spell_mastery_icicles_hit() : SpellScriptLoader("spell_mastery_icicles_hit") { }

        class spell_mastery_icicles_hit_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mastery_icicles_hit_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (target->GetGUID() != _player->GetGUID())
                        {
                            int8 currentMinAura = 0;
                            if (IcicleOverstack == false)
                            {
                                // We need to find the first icicle and if we found it - break, because function will be called one more time.
                                for (int i = 4; i >= 0; i--)
                                {
                                    if (AuraPtr icicleCurrentAura = _player->GetAura(IcicleAuras[i]))
                                    {
                                        int32 basepoints = icicleCurrentAura->GetEffect(0)->GetAmount();
                                        _player->CastSpell(target, IcicleHits[i], true);
                                        _player->CastCustomSpell(target, SPELL_MAGE_ICICLE_DAMAGE, &basepoints, NULL, NULL, true);
                                        _player->RemoveAura(IcicleAuras[i]);
                                        break;
                                    }
                                }
                            }

                            IcicleOverstack = false;
                        }
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mastery_icicles_hit_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mastery_icicles_hit_SpellScript();
        }
};

// Mastery: Echo of Light - 77485
class spell_mastery_echo_of_light : public SpellScriptLoader
{
    public:
        spell_mastery_echo_of_light() : SpellScriptLoader("spell_mastery_echo_of_light") { }

        class spell_mastery_echo_of_light_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mastery_echo_of_light_AuraScript);

            void OnProc(constAuraEffectPtr aurEff, ProcEventInfo& eventInfo)
            {
                PreventDefaultAction();

                if (!GetCaster())
                    return;

                if (!eventInfo.GetHealInfo() || !eventInfo.GetHealInfo()->GetHeal())
                    return;

                Unit* unitTarget = eventInfo.GetActionTarget();
                Player* plr = GetCaster()->ToPlayer();
                if (!unitTarget || !plr)
                    return;

                float Mastery = plr->GetFloatValue(PLAYER_MASTERY) * 1.25f / 100.0f;
                int32 bp = (Mastery * eventInfo.GetHealInfo()->GetHeal()) / 6;

                bp += unitTarget->GetRemainingPeriodicAmount(plr->GetGUID(), SPELL_PRIEST_ECHO_OF_LIGHT, SPELL_AURA_PERIODIC_HEAL);
                plr->CastCustomSpell(unitTarget, SPELL_PRIEST_ECHO_OF_LIGHT, &bp, NULL, NULL, true);
            }

            void Register()
            {
                OnEffectProc += AuraEffectProcFn(spell_mastery_echo_of_light_AuraScript::OnProc, EFFECT_0, SPELL_AURA_DUMMY);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mastery_echo_of_light_AuraScript();
        }
};

void AddSC_mastery_spell_scripts()
{
    new spell_mastery_shield_discipline();
    new spell_mastery_blood_shield();
    new spell_mastery_ignite();
    new spell_mastery_hand_of_light();
    new spell_mastery_icicles();
    new spell_mastery_icicles_trigger();
    new spell_mastery_icicles_periodic();
    new spell_mastery_icicles_hit();
    new spell_mastery_echo_of_light();
}
