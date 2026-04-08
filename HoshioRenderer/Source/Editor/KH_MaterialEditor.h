#pragma once

#include "KH_Panel.h"

class KH_MaterialEditor : public KH_Panel
{
public:
	KH_MaterialEditor() = default;
	~KH_MaterialEditor() override = default;

	void Render() override;

private:
	int SelectedMaterialID = 0;

	int LastSyncedObjectID = -2;
	int LastSyncedMeshID = -2;

	void SyncSelectedMaterialFromEditorSelection();
};

