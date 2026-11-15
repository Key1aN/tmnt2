#pragma once


/*
 * Debug Tools add-on tab "Remaster" for Enhanced Graphics v1.1.
 *
 * Same interface as the Combat Overhaul tab, so both menu front ends (game font
 * and ImGui) and every input path (keyboard, mouse, controller: D-pad / stick to
 * move and change, A run, X reset item, Y reset tab, LB/RB tabs) work unchanged.
 * The tab only exists while the enhanced-graphics package is enabled.
 */
class CEnhancedGraphicsTab
{
public:
    static bool IsVisible(void);
    static int32 GetItemNum(void);
    static bool IsNumericItem(int32 index);
    static bool GetItemFraction(int32 index, float* pfFraction);
    static bool GetItemRange(int32 index, float* pfMin, float* pfMax, float* pfValue);
    static void SetItemValue(int32 index, float fValue);
    static bool IsReadoutItem(int32 index);
    static const char* GetItemHeader(int32 index);
    static void FormatItem(int32 index, char* pszBuffer, size_t capacity);
    static void GetItemHelp(int32 index, const char*& rpszScope, const char*& rpszDescription);
    static void AdjustItem(int32 index, int32 direction, int32 multiplier);
    static void ActivateItem(int32 index);
    static void ResetItem(int32 index);
    static void ResetAll(void);
};
