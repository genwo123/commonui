// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonTabListWidgetBase.h"
#include "CommonUIPrivatePCH.h"

#include "CommonUISubsystemBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonAnimatedSwitcher.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ICommonUIModule.h"
#include "CommonUIUtils.h"
#include "Input/CommonUIInputTypes.h"

UCommonTabListWidgetBase::UCommonTabListWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAutoListenForInput(false)
	, TabButtonGroup(nullptr)
	, bIsListeningForInput(false)
	, RegisteredTabsByID()
	, ActiveTabID(NAME_None)
{
}
 
void UCommonTabListWidgetBase::SetLinkedSwitcher(UCommonAnimatedSwitcher* CommonSwitcher)
{
	if (LinkedSwitcher.Get() != CommonSwitcher)
	{
		HandlePreLinkedSwitcherChanged();
		LinkedSwitcher = CommonSwitcher;
		HandlePostLinkedSwitcherChanged();
	}
}

UCommonAnimatedSwitcher* UCommonTabListWidgetBase::GetLinkedSwitcher() const
{
	return LinkedSwitcher.Get();
}

bool UCommonTabListWidgetBase::RegisterTab(FName TabNameID, TSubclassOf<UCommonButtonBase> ButtonWidgetType, UWidget* ContentWidget)
{
	bool AreParametersValid = true;

	// Early out on redundant tab registration.
	if (!ensure(!RegisteredTabsByID.Contains(TabNameID)))
	{
		AreParametersValid = false;
	}

	// Early out on invalid tab button type.
	if (!ensure(ButtonWidgetType))
	{
		AreParametersValid = false;
	}

	if (!AreParametersValid)
	{
		return false;
	}

	UCommonButtonBase* const NewTabButton = CreateWidget<UCommonButtonBase>(GetOwningPlayer(), ButtonWidgetType);
	if (!ensureMsgf(NewTabButton, TEXT("Failed to create tab button. Aborting tab registration.")))
	{
		return false;
	}

	// Tab book-keeping.
	FCommonRegisteredTabInfo NewTabInfo;
	NewTabInfo.TabIndex = RegisteredTabsByID.Num();
	NewTabInfo.TabButton = NewTabButton;
	NewTabInfo.ContentInstance = ContentWidget;
	RegisteredTabsByID.Add(TabNameID, NewTabInfo);

	// Enforce the "contract" that tab buttons require - single-selectability, but not toggleability.
	NewTabButton->SetIsSelectable(true);
	NewTabButton->SetIsToggleable(false);
	// NOTE: Adding the button to the group may change it's selection, which raises an event we listen to,
	// which can only properly be handled if we already know that this button is associated with a registered tab.
	if (ensure(TabButtonGroup != nullptr))
	{
		TabButtonGroup->AddWidget(NewTabButton);
	}

	// Callbacks.
	HandleTabCreation(TabNameID, NewTabInfo.TabButton);
	OnTabButtonCreation.Broadcast(TabNameID, NewTabInfo.TabButton);

	return true;
}


bool UCommonTabListWidgetBase::RemoveTab(FName TabNameID)
{
	FCommonRegisteredTabInfo* const TabInfo = RegisteredTabsByID.Find(TabNameID);
	if (!TabInfo)
	{
		return false;
	}

	UCommonButtonBase* const TabButton = TabInfo->TabButton;
	if (TabButton)
	{
		TabButtonGroup->RemoveWidget(TabButton);
		TabButton->RemoveFromParent();
	}
	RegisteredTabsByID.Remove(TabNameID);

	// Callbacks
	HandleTabRemoval(TabNameID, TabButton);
	OnTabButtonRemoval.Broadcast(TabNameID, TabButton);

	return true;
}

void UCommonTabListWidgetBase::RemoveAllTabs()
{
	for (TMap<FName, FCommonRegisteredTabInfo>::TIterator Iter(RegisteredTabsByID); Iter; ++Iter)
	{
		RemoveTab(Iter->Key);
	}
}

int32 UCommonTabListWidgetBase::GetTabCount() const
{
	// TODO Should we ensure all the tabs in the list are valid?
	return RegisteredTabsByID.Num();
}

void UCommonTabListWidgetBase::SetListeningForInput(bool bShouldListen)
{
	if (bShouldListen && !TabButtonGroup)
	{
		// If there's no tab button group, it means we haven't been constructed and we shouldn't listen to anything
		return;
	}

	if (GetUISubsystem() == nullptr)
	{
		// Shutting down
		return;
	}

	if (bShouldListen != bIsListeningForInput)
	{
		bIsListeningForInput = bShouldListen;
		UpdateBindings();
	}
}

void UCommonTabListWidgetBase::UpdateBindings()
{
	// New input system binding flow
	if (bIsListeningForInput)
	{
		NextTabActionHandle = RegisterUIActionBinding(FBindUIActionArgs(NextTabInputActionData, false, FSimpleDelegate::CreateUObject(this, &UCommonTabListWidgetBase::HandleNextTabAction)));
		PrevTabActionHandle = RegisterUIActionBinding(FBindUIActionArgs(PreviousTabInputActionData, false, FSimpleDelegate::CreateUObject(this, &UCommonTabListWidgetBase::HandlePreviousTabAction)));
	}
	else
	{
		NextTabActionHandle.Unregister();
		PrevTabActionHandle.Unregister();
	}
}

bool UCommonTabListWidgetBase::SelectTabByID(FName TabNameID, bool bSuppressClickFeedback)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->SetIsSelected(true, !bSuppressClickFeedback);
			return true;
		}
	}

	return false;
}

FName UCommonTabListWidgetBase::GetSelectedTabId() const
{
	FName FoundId = NAME_None;

	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Value.TabButton != nullptr && TabPair.Value.TabButton->GetSelected())
		{
			FoundId = TabPair.Key;
			break;
		}
	}

	return FoundId;
}

FName UCommonTabListWidgetBase::GetTabIdAtIndex(int32 Index) const
{
	FName FoundId = NAME_None;

	if (ensure(Index < RegisteredTabsByID.Num()))
	{
		for (auto& TabPair : RegisteredTabsByID)
		{
			if (TabPair.Value.TabIndex == Index)
			{
				FoundId = TabPair.Key;
				break;
			}
		}
	}

	return FoundId;
}

void UCommonTabListWidgetBase::SetTabVisibility(FName TabNameID, ESlateVisibility NewVisibility)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->SetVisibility(NewVisibility);
			
			if (NewVisibility == ESlateVisibility::Collapsed || NewVisibility == ESlateVisibility::Hidden)
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(false);
			}
			else
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(true);
			}
			
			break;
		}
	}
}

void UCommonTabListWidgetBase::SetTabEnabled(FName TabNameID, bool bEnable)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			if (bEnable)
			{
				TabPair.Value.TabButton->SetIsEnabled(true);
			}
			else
			{
				TabPair.Value.TabButton->SetIsEnabled(false);
			}

			break;
		}
	}
}

void UCommonTabListWidgetBase::SetTabInteractionEnabled(FName TabNameID, bool bEnable)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			if (bEnable)
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(true);
			}
			else
			{
				TabPair.Value.TabButton->SetIsInteractionEnabled(false);
			}

			break;
		}
	}
}

void UCommonTabListWidgetBase::DisableTabWithReason(FName TabNameID, const FText& Reason)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		if (TabPair.Key == TabNameID && ensure(TabPair.Value.TabButton))
		{
			TabPair.Value.TabButton->DisableButtonWithReason(Reason);
			break;
		}
	}
}

UCommonButtonBase* UCommonTabListWidgetBase::GetTabButtonBaseByID(FName TabNameID)
{
	if (FCommonRegisteredTabInfo* TabInfo = RegisteredTabsByID.Find(TabNameID))
	{
		return TabInfo->TabButton;
	}

	return nullptr;
}

void UCommonTabListWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Create the button group once up-front
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);
	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UCommonTabListWidgetBase::HandleTabButtonSelected);
}

void UCommonTabListWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (bAutoListenForInput)
	{
		SetListeningForInput(true);
	}
}

void UCommonTabListWidgetBase::NativeDestruct()
{
	Super::NativeDestruct();

	SetListeningForInput(false);

	ActiveTabID = NAME_None;
	RemoveAllTabs();
	TabButtonGroup->RemoveAll();
}

void UCommonTabListWidgetBase::HandlePreLinkedSwitcherChanged()
{
	HandlePreLinkedSwitcherChanged_BP();
}

void UCommonTabListWidgetBase::HandlePostLinkedSwitcherChanged()
{
	HandlePostLinkedSwitcherChanged_BP();
}

void UCommonTabListWidgetBase::HandleTabCreation_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
}

void UCommonTabListWidgetBase::HandleTabRemoval_Implementation(FName TabNameID, UCommonButtonBase* TabButton)
{
}

void UCommonTabListWidgetBase::HandleTabButtonSelected(UCommonButtonBase* SelectedTabButton, int32 ButtonIndex)
{
	for (auto& TabPair : RegisteredTabsByID)
	{
		FCommonRegisteredTabInfo& TabInfo = TabPair.Value;
			
		if (TabInfo.TabButton == SelectedTabButton)
		{
			ActiveTabID = TabPair.Key;

			if (TabInfo.ContentInstance || LinkedSwitcher.IsValid())
			{
				if (ensureMsgf(TabInfo.ContentInstance, TEXT("A CommonTabListWidget tab button lacks a tab content widget to set its linked switcher to.")) &&
					ensureMsgf(LinkedSwitcher.IsValid(), TEXT("A CommonTabListWidgetBase.has a registered tab with a content widget to switch to, but has no linked activatable widget switcher. Did you forget to call SetLinkedSwitcher to establish the association?")))
				{
					// There's already an instance of the widget to display, so go for it
					LinkedSwitcher->SetActiveWidget(TabInfo.ContentInstance);
				}
			}

			OnTabSelected.Broadcast(TabPair.Key);
		}
	}
}

void UCommonTabListWidgetBase::HandleNextTabInputAction(bool& bPassThrough)
{
	HandleNextTabAction();
}

void UCommonTabListWidgetBase::HandleNextTabAction()
{
	if (ensure(TabButtonGroup))
	{
		TabButtonGroup->SelectNextButton();
	}
}

void UCommonTabListWidgetBase::HandlePreviousTabInputAction(bool& bPassThrough)
{
	HandlePreviousTabAction();
}

void UCommonTabListWidgetBase::HandlePreviousTabAction()
{
	if (ensure(TabButtonGroup))
	{
		TabButtonGroup->SelectPreviousButton();
	}
}