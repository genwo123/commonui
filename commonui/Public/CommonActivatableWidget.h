// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "CommonActivatableWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWidgetActivationChanged);

/** 
 * The base for widgets that are capable of being "activated" and "deactivated" during their lifetime without being otherwise modified or destroyed. 
 *
 * This is generally desired for one or more of the following purposes:
 *	- This widget can turn on/off without being removed from the hierarchy (or otherwise reconstructing the underlying SWidgets), so Construct/Destruct are insufficient
 *	- You'd like to be able to "go back" from this widget, whether that means back a breadcrumb, closing a modal, or something else. This is built-in here.
 *	- This widget's place in the hierarchy is such that it defines a meaningful node-point in the tree of activatable widgets through which input is routed to all widgets.
 *
 * By default, an activatable widget:
 *	- Is not automatically activated upon construction
 *	- Does not register to receive back actions (or any other actions, for that matter)
 *	- If classified as a back handler, is automatically deactivated (but not destroyed) when it receives a back action
 * 
 * Note that removing an activatable widget from the UI (i.e. triggering Destruct()) will always deactivate it, even if the UWidget is not destroyed.
 * Re-constructing the underlying SWidget will only result in re-activation if auto-activate is enabled.
 *
 * TODO: ADD MORE INFO ON INPUTS
 */
UCLASS()
class COMMONUI_API UCommonActivatableWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = ActivatableWidget)
	bool IsActivated() const { return bIsActive; }

	UFUNCTION(BlueprintCallable, Category = ActivatableWidget)
	void ActivateWidget();

	UFUNCTION(BlueprintCallable, Category = ActivatableWidget)
	void DeactivateWidget();

	FSimpleMulticastDelegate& OnActivated() const { return OnActivatedEvent; }
	FSimpleMulticastDelegate& OnDeactivated() const { return OnDeactivatedEvent; }

//COMMONUI_SCOPE:
public:

	/** Returns the desired widget to focus when this Widget Activates. */
	UWidget* GetDesiredFocusTarget() const;
	
	virtual TOptional<FUICameraConfig> GetDesiredCameraConfig() const;
	/**
	 * Gets the desired input configuration to establish when this widget activates and can receive input (i.e. all parents are also active).
	 * This configuration will override the existing one established by any previous activatable widget and restore it (if valid) upon deactivation.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const;

	bool IsModal() const { return bIsModal; }
	bool SupportsActivationFocus() const { return bSupportsActivationFocus; }
	bool AutoRestoresFocus() const { return bSupportsActivationFocus && bAutoRestoreFocus; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FActivatableWidgetRebuildEvent, UCommonActivatableWidget&);
	static FActivatableWidgetRebuildEvent OnRebuilding;
	
	FSimpleMulticastDelegate& OnSlateReleased() const { return OnSlateReleasedEvent; }

	FSimpleMulticastDelegate& OnRequestRefreshFocus() const { return OnRequestRefreshFocusEvent; }

	bool SetsVisibilityOnActivated() const { return bSetVisibilityOnActivated; }
	bool SetsVisibilityOnDeactivated() const { return bSetVisibilityOnDeactivated; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Override to provide the desired widget that should receive focus when this becomes the primary active widget.
	 * If bAutoRestoreFocus is true, is only called when there is no valid cached restoration target (to provide the default/fallback)
	 */
	virtual UWidget* NativeGetDesiredFocusTarget() const;
	
	/** 
	 * Implement to provide the desired widget to focus if/when this activatable becomes the primary active widget.
	 * Note: This is a fallback used only if the native class parentage does not provide a target.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = ActivatableWidget, meta = (DisplayName = "Get Desired Focus Target"))
	UWidget* BP_GetDesiredFocusTarget() const;

	/**
	 * Ask for focus to be re-set to our current DesiredFocusTarget,
	 * but only if our node is currently the leaf-most active node (no stealing!).
	 * This is useful for complex cases like: the buttons animate in from off-screen,
	 * or the buttons are deeply nested in a multi-switcher hierarchy and it would be burdensome
	 * to wrap each element in a CommonActivatableWidget.
	 */
	void RequestRefreshFocus();

	UFUNCTION(BlueprintImplementableEvent, Category = ActivatableWidget, meta = (DisplayName = "On Activated"))
	void BP_OnActivated();
	virtual void NativeOnActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = ActivatableWidget, meta = (DisplayName = "On Deactivated"))
	void BP_OnDeactivated();
	virtual void NativeOnDeactivated();

	/** 
	 * Override in BP implementations to provide custom behavior when receiving a back action 
	 * Note: Only called if native code in the base class hasn't handled it in NativeOnHandleBackAction 
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = ActivatableWidget, meta = (DisplayName = "On Handle Back Action"))
	bool BP_OnHandleBackAction();
	virtual bool NativeOnHandleBackAction();

	void HandleBackAction();

	/** True to automatically activate upon construction */
	UPROPERTY(EditAnywhere, Category = Activation)
	bool bAutoActivate = false;

	/** True to receive "Back" actions automatically. Custom back handler behavior can be provided, default is to deactivate. */
	UPROPERTY(EditAnywhere, Category = Activation)
	bool bIsBackHandler = false;

	//@todo DanH: This property name suuucks, need to circle back and pick something better
	/**
	 * True if this widget is a candidate to receive/route focus or specify a desired UIInputConfig when active.
	 * Primary reason for disabling is for utility sub-widgets within a larger screen that possess actions, but are never
	 * intended to be involved in navigation or dictate changes to the active UI input config.
	 */
	UPROPERTY(EditAnywhere, Category = Activation)
	bool bSupportsActivationFocus = true;

	/** 
	 * True to have this widget be treated as a root node for input routing, regardless of its actual parentage.
	 * Should seldom be needed, but useful in cases where a child widget should prevent all action processing by parents, even though they remain active (ex: modal popup menu).
	 */
	UPROPERTY(EditAnywhere, Category = Activation, meta = (EditCondition = bSupportsActivationFocus))
	bool bIsModal = false;

	/** 
	 * True to prefer automatically restoring focus to the widget that was focused when this widget last became the non-leafmost-active-widget.
	 * If true and a valid restoration candidate exists, we'll use that. If it doesn't, we rely on GetDesiredFocusTarget()
	 * If false, we simply always rely on GetDesiredFocusTarget()
	 */
	UPROPERTY(EditAnywhere, Category = Activation, meta = (EditCondition = bSupportsActivationFocus))
	bool bAutoRestoreFocus = false;
	
private:
	UPROPERTY(EditAnywhere, Category = Activation, meta = (InlineEditConditionToggle = "ActivatedVisibility"))
	bool bSetVisibilityOnActivated = false;

	UPROPERTY(EditAnywhere, Category = Activation, meta = (EditCondition = "bSetVisibilityOnActivated"))
	ESlateVisibility ActivatedVisibility = ESlateVisibility::SelfHitTestInvisible;

	UPROPERTY(EditAnywhere, Category = Activation, meta = (InlineEditConditionToggle = "DeactivatedVisibility"))
	bool bSetVisibilityOnDeactivated = false;

	UPROPERTY(EditAnywhere, Category = Activation, meta = (EditCondition = "bSetVisibilityOnDeactivated"))
	ESlateVisibility DeactivatedVisibility = ESlateVisibility::Collapsed;

	/** Fires when the widget is activated. */
	UPROPERTY(BlueprintAssignable, Category = Events, meta = (AllowPrivateAccess = true, DisplayName = "On Widget Activated"))
	FOnWidgetActivationChanged BP_OnWidgetActivated;
	
	/** Fires when the widget is deactivated. */
	UPROPERTY(BlueprintAssignable, Category = Events, meta = (AllowPrivateAccess = true, DisplayName = "On Widget Deactivated"))
	FOnWidgetActivationChanged BP_OnWidgetDeactivated;
	
	UPROPERTY(BlueprintReadOnly, Category = ActivatableWidget, meta = (AllowPrivateAccess = true))
	bool bIsActive = false;

	mutable FSimpleMulticastDelegate OnActivatedEvent;
	mutable FSimpleMulticastDelegate OnDeactivatedEvent;
	mutable FSimpleMulticastDelegate OnSlateReleasedEvent;
	mutable FSimpleMulticastDelegate OnRequestRefreshFocusEvent;

protected:
	virtual void InternalProcessActivation();
	virtual void InternalProcessDeactivation();
	void Reset();
};