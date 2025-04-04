// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "CommonInputBaseTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateTypes.h"
#include "CommonInputSubsystem.generated.h"

class UWidget;
class ULocalPlayer;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputMethodChangedDelegate, ECommonInputType, bNewInputType);

UCLASS(DisplayName = "CommonInput")
class COMMONINPUT_API UCommonInputSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UCommonInputSubsystem* Get(const ULocalPlayer* LocalPlayer);

	UCommonInputSubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	DECLARE_EVENT_OneParam(UCommonInputSubsystem, FInputMethodChangedEvent, ECommonInputType);
	FInputMethodChangedEvent OnInputMethodChangedNative;

	void SetInputTypeFilter(ECommonInputType InputType, FName Reason, bool Filter);
	bool GetInputTypeFilter(ECommonInputType InputType) const;

	/**  */
	void AddOrRemoveInputTypeLock(FName InReason, ECommonInputType InInputType, bool bAddLock);

	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	bool IsInputMethodActive(ECommonInputType InputMethod) const;

	/** The current input type based on the last input received on the device. */
	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	ECommonInputType GetCurrentInputType() const;

	/** The default input type for the current platform. */
	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	ECommonInputType GetDefaultInputType() const;

	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	void SetCurrentInputType(ECommonInputType NewInputType);

	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	const FName GetCurrentGamepadName() const;

	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	void SetGamepadInputType(const FName& InGamepadInputType);

	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	bool IsUsingPointerInput() const;

	/** Should display indicators for the current input device on screen.  This is needed when capturing videos, but we don't want to reveal the capture source device. */
	UFUNCTION(BlueprintCallable, Category = CommonInputSubsystem)
	bool ShouldShowInputKeys() const;

	/** Returns true if the specified key can be present on both a mobile device and mobile gamepads */
	static bool IsMobileGamepadKey(const FKey& InKey);

	/** Returns true if the current platform supports a hardware cursor */
	bool PlatformSupportsHardwareCursor() const;

	void SetCursorPosition(FVector2D NewPosition, bool bForce);

	void UpdateCursorPosition(TSharedRef<FSlateUser> SlateUser, const FVector2D& NewPosition, bool bForce = false);

	/** Getter */
	bool GetIsGamepadSimulatedClick() const;

	/** Setter */
	void SetIsGamepadSimulatedClick(bool bNewIsGamepadSimulatedClick);

protected:
	ECommonInputType LockInput(ECommonInputType InputToLock) const;

	void BroadcastInputMethodChanged();

private:
	bool Tick(float DeltaTime);

	void ShouldShowInputKeysChanged(IConsoleVariable* Var);

	FVector2D ClampPositionToViewport(const FVector2D& InPosition) const;

	/** Returns true if the current platform supports the input type */
	bool PlatformSupportsInputType(ECommonInputType InInputType) const;

	bool CheckForInputMethodThrashing(ECommonInputType NewInputType);

	FDelegateHandle TickHandle;

	UPROPERTY(BlueprintAssignable, Category = CommonInputSubsystem, meta = (AllowPrivateAccess))
	FInputMethodChangedDelegate OnInputMethodChanged;

	UPROPERTY(Transient)
	int32 NumberOfInputMethodChangesRecently = 0;

	UPROPERTY(Transient)
	double LastInputMethodChangeTime = 0;

	UPROPERTY(Transient)
	double LastTimeInputMethodThrashingBegan = 0;

	/**  */
	UPROPERTY(Transient)
	ECommonInputType LastInputType;

	/**  */
	UPROPERTY(Transient)
	ECommonInputType CurrentInputType;

	/**  */
	UPROPERTY(Transient)
	FName GamepadInputType;

	/**  */
	UPROPERTY(Transient)
	TMap<FName, ECommonInputType> CurrentInputLocks;

	TOptional<ECommonInputType> CurrentInputLock;

	TSharedPtr<class FCommonInputPreprocessor> CommonInputPreprocessor;

	/** Is the current click simulated by the gamepad's face button down/right (platform dependent) */
	UPROPERTY(Transient)
	bool bIsGamepadSimulatedClick;
};