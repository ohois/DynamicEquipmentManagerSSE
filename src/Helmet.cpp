#include "Helmet.h"

#include "skse64_common/Relocation.h"  // RelocPtr
#include "skse64_common/SafeWrite.h"  // SafeWrite64

#include <type_traits>  // typeid

#include "Animations.h"  // Anim, HashAnimation
#include "Forms.h"  // WerewolfBeastRace, DLC1VampireBeastRace

#include "RE/Skyrim.h"
#include "SKSE/API.h"


namespace Helmet
{
	Helmet* Helmet::GetSingleton()
	{
		static Helmet singleton;
		return &singleton;
	}


	void Helmet::Clear()
	{
		ISerializableForm::Clear();
		_enchantment.Clear();
	}


	bool Helmet::Save(SKSE::SerializationInterface* a_intfc, UInt32 a_type, UInt32 a_version)
	{
		if (!ISerializableForm::Save(a_intfc, a_type, a_version)) {
			return false;
		} else if (!_enchantment.Save(a_intfc)) {
			return false;
		} else {
			return true;
		}
	}


	bool Helmet::Load(SKSE::SerializationInterface* a_intfc)
	{
		if (!ISerializableForm::Load(a_intfc)) {
			return false;
		} else if (!_enchantment.Load(a_intfc)) {
			return false;
		} else {
			return true;
		}
	}


	RE::TESObjectARMO* Helmet::GetForm()
	{
		return static_cast<RE::TESObjectARMO*>(ISerializableForm::GetForm());
	}


	void Helmet::SetEnchantmentForm(UInt32 a_formID)
	{
		_enchantment.SetForm(a_formID);
	}


	RE::EnchantmentItem* Helmet::GetEnchantmentForm()
	{
		return _enchantment.GetForm();
	}


	UInt32 Helmet::GetEnchantmentFormID()
	{
		return _enchantment.GetFormID();
	}


	RE::EnchantmentItem* Helmet::Enchantment::GetForm()
	{
		return static_cast<RE::EnchantmentItem*>(ISerializableForm::GetForm());
	}


	void HelmetTaskDelegate::Run()
	{
		if (_equip) {
			HelmetEquipVisitor visitor;
			VisitPlayerInventoryChanges(&visitor);
		} else {
			HelmetUnEquipVisitor visitor;
			VisitPlayerInventoryChanges(&visitor);
		}
	}


	void HelmetTaskDelegate::Dispose()
	{
		delete this;
	}


	DelayedHelmetLocator::Visitor::Visitor(UInt32 a_formID) :
		_formID(a_formID)
	{}


	bool HelmetTaskDelegate::HelmetEquipVisitor::Accept(RE::InventoryEntryData* a_entry, SInt32 a_count)
	{
		auto helmet = Helmet::GetSingleton();
		if (a_entry->type->formID == helmet->GetFormID()) {
			auto armor = static_cast<RE::TESObjectARMO*>(a_entry->type);
			auto enchantment = helmet->GetEnchantmentForm();
			if (enchantment) {
				if (!a_entry->extraList) {
					return true;
				}
				bool found = false;
				for (auto& xList : *a_entry->extraList) {
					auto xEnch = xList->GetByType<RE::ExtraEnchantment>();
					if (xEnch && xEnch->objectEffect && xEnch->objectEffect->formID == enchantment->formID) {
						found = true;
						break;
					}
				}
				if (!found) {
					return true;
				}
			}
			auto equipManager = RE::EquipManager::GetSingleton();
			auto player = RE::PlayerCharacter::GetSingleton();
			auto xList = (a_entry->extraList && !a_entry->extraList->empty()) ? a_entry->extraList->front() : 0;
			equipManager->EquipItem(player, armor, xList, 1, armor->equipmentType, true, false, false);
			return false;
		}
		return true;
	}


	bool HelmetTaskDelegate::HelmetUnEquipVisitor::Accept(RE::InventoryEntryData* a_entry, SInt32 a_count)
	{
		using FirstPersonFlag = RE::BGSBipedObjectForm::BipedBodyTemplate::FirstPersonFlag;

		if (a_entry->type && a_entry->type->Is(RE::FormType::Armor) && a_entry->extraList) {
			for (auto& xList : *a_entry->extraList) {
				if (xList->HasType(RE::ExtraDataType::kWorn)) {
					auto armor = static_cast<RE::TESObjectARMO*>(a_entry->type);
					if (armor->HasPartOf(FirstPersonFlag::kHair) && (armor->IsLightArmor() || armor->IsHeavyArmor() || armor->IsClothing())) {
						auto equipManager = RE::EquipManager::GetSingleton();
						auto player = RE::PlayerCharacter::GetSingleton();
						equipManager->UnEquipItem(player, armor, xList, 1, armor->equipmentType, true, false);
						return false;
					}
				}
			}
		}
		return true;
	}


	HelmetTaskDelegate::HelmetTaskDelegate(bool a_equip) :
		_equip(a_equip)
	{}


	DelayedHelmetLocator::DelayedHelmetLocator(UInt32 a_formID) :
		_formID(a_formID)
	{}


	void DelayedHelmetLocator::Run()
	{
		Visitor visitor(_formID);
		VisitPlayerInventoryChanges(&visitor);
	}


	void DelayedHelmetLocator::Dispose()
	{
		delete this;
	}


	bool DelayedHelmetLocator::Visitor::Accept(RE::InventoryEntryData* a_entry, SInt32 a_count)
	{
		if (a_entry->type->formID == _formID && a_entry->extraList) {
			for (auto& xList : *a_entry->extraList) {
				if (xList->HasType(RE::ExtraDataType::kWorn)) {
					auto helmet = Helmet::GetSingleton();
					helmet->Clear();
					helmet->SetForm(_formID);
					auto armor = static_cast<RE::TESObjectARMO*>(a_entry->type);
					for (auto& xList : *a_entry->extraList) {
						if (xList->HasType(RE::ExtraDataType::kEnchantment)) {
							auto ench = xList->GetByType<RE::ExtraEnchantment>();
							if (ench && ench->objectEffect) {
								helmet->SetEnchantmentForm(ench->objectEffect->formID);
							}
						}
					}
					return false;
				}
			}
		}
		return true;
	}


	void AnimGraphSinkDelegate::Run()
	{
		SinkAnimationGraphEventHandler(BSAnimationGraphEventHandler::GetSingleton());
	}


	void AnimGraphSinkDelegate::Dispose()
	{
		delete this;
	}


	TESEquipEventHandler* TESEquipEventHandler::GetSingleton()
	{
		static TESEquipEventHandler singleton;
		return &singleton;
	}


	auto TESEquipEventHandler::ReceiveEvent(RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource)
		-> EventResult
	{
		using FirstPersonFlag = RE::BGSBipedObjectForm::BipedBodyTemplate::FirstPersonFlag;

		if (!a_event || !a_event->akSource || !a_event->akSource->IsPlayerRef() || PlayerIsBeastRace()) {
			return EventResult::kContinue;
		}

		auto armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(a_event->formID);
		if (!armor) {
			return EventResult::kContinue;
		}

		if (armor->HasPartOf(FirstPersonFlag::kHead | FirstPersonFlag::kHair | FirstPersonFlag::kCirclet)) {
			auto helmet = Helmet::GetSingleton();
			if (armor->IsLightArmor() || armor->IsHeavyArmor() || armor->IsClothing()) {
				if (a_event->isEquipping) {
					SKSE::GetTaskInterface()->AddTask(new DelayedHelmetLocator(armor->formID));
				} else {
					auto player = RE::PlayerCharacter::GetSingleton();
					if (player->IsWeaponDrawn()) {
						helmet->Clear();
					}
				}
			} else {
				helmet->Clear();
			}
		}

		return EventResult::kContinue;
	}


	BSAnimationGraphEventHandler* BSAnimationGraphEventHandler::GetSingleton()
	{
		static BSAnimationGraphEventHandler singleton;
		return &singleton;
	}


	auto BSAnimationGraphEventHandler::ReceiveEvent(RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
		-> EventResult
	{
		if (!a_event || !a_event->akTarget || !a_event->akTarget->IsPlayerRef()) {
			return EventResult::kContinue;
		}

		auto task = SKSE::GetTaskInterface();
		switch (HashAnimation(a_event->animName)) {
		case Anim::kWeaponDraw:
			if (!PlayerIsBeastRace()) {
				task->AddTask(new HelmetTaskDelegate(true));
			}
			break;
		case Anim::kWeaponSheathe:
			if (!PlayerIsBeastRace()) {
				task->AddTask(new HelmetTaskDelegate(false));
			}
			break;
		case Anim::kGraphDeleting:
			task->AddTask(new AnimGraphSinkDelegate());
			break;
		}

		return EventResult::kContinue;
	}
}
