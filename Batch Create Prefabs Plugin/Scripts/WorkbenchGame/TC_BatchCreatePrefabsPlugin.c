/**
 * TC_BatchCreatePrefabsPlugin.c
 * Batch-creates a flat .et prefab for each selected entity in the World Editor.
 * Works on base-game prefab instances by spawning a temporary entity from the
 * ancestor prefab, saving it as a template, then deleting the temp entity.
 *
 * Usage:
 *   1. Select one or more entities in the World Editor.
 *   2. Press Ctrl+Shift+B (or Plugins > Batch Create Prefabs).
 *   3. Confirm/change the output folder and click Run Batch.
 *
 * Configure:
 *   Plugins > Settings > Batch Create Prefabs
 *   Set "Output Folder" to an addon-relative path, e.g. Prefabs/Generated
 */

[WorkbenchPluginAttribute(
	name: "Batch Create Prefabs",
	description: "Creates a flat .et prefab file for each selected entity using its scene name as filename.",
	shortcut: "Ctrl+Shift+B",
	wbModules: {"WorldEditor"},
	awesomeFontCode: 0xF0C5
)]
class TC_BatchCreatePrefabsPlugin : WorldEditorPlugin
{
	[Attribute(defvalue: "Prefabs/Generated", desc: "Addon-relative folder where .et files will be saved (e.g. Prefabs/Generated)")]
	string m_sOutputFolder;

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		// Show settings dialog so user can confirm/change output folder before running
		Workbench.ScriptDialog("Batch Create Prefabs", "Set the addon-relative output folder.\nIt will be created automatically if it does not exist.\n\nClick 'Run Batch' to proceed.", this);
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Run Batch")]
	void RunBatch()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
		{
			Print("[BatchCreatePrefabs] ERROR: WorldEditor module not available.", LogLevel.ERROR);
			return;
		}

		WorldEditorAPI api = worldEditor.GetApi();
		if (!api)
		{
			Print("[BatchCreatePrefabs] ERROR: WorldEditorAPI not available.", LogLevel.ERROR);
			return;
		}

		int selectedCount = api.GetSelectedEntitiesCount();
		if (selectedCount == 0)
		{
			Print("[BatchCreatePrefabs] No entities selected. Select one or more entities and run again.", LogLevel.WARNING);
			return;
		}

		// Resolve addon root to absolute path
		string addonRoot;
		if (!Workbench.GetAbsolutePath("$TESTINGCLAUD:", addonRoot, false))
		{
			Print("[BatchCreatePrefabs] ERROR: Could not resolve addon root. Make sure the addon is loaded.", LogLevel.ERROR);
			return;
		}

		// Build absolute output folder path using forward slashes (Workbench style)
		addonRoot.Replace("\\", "/");
		while (addonRoot.EndsWith("/"))
			addonRoot = addonRoot.Substring(0, addonRoot.Length() - 1);

		string relFolder = m_sOutputFolder;
		relFolder.Replace("\\", "/");
		while (relFolder.StartsWith("/"))
			relFolder = relFolder.Substring(1, relFolder.Length() - 1);

		string absOutputFolder = addonRoot + "/" + relFolder + "/";

		// Create the folder if it does not exist
		if (!FileIO.FileExists(absOutputFolder))
		{
			Print("[BatchCreatePrefabs] Folder does not exist, creating: " + absOutputFolder, LogLevel.NORMAL);
			if (!FileIO.MakeDirectory(absOutputFolder))
			{
				Print("[BatchCreatePrefabs] ERROR: Failed to create folder: " + absOutputFolder + ". Try creating it manually.", LogLevel.ERROR);
				return;
			}
			Print("[BatchCreatePrefabs] Folder created successfully.", LogLevel.NORMAL);
		}

		Print("[BatchCreatePrefabs] Output folder: " + absOutputFolder, LogLevel.NORMAL);
		Print("[BatchCreatePrefabs] Processing " + selectedCount + " selected entities...", LogLevel.NORMAL);

		// Collect entity names and sources before modifying the scene
		array<string> entityNames = {};
		array<IEntitySource> entitySources = {};

		for (int i = 0; i < selectedCount; i++)
		{
			IEntitySource entSrc = api.GetSelectedEntity(i);
			if (!entSrc)
			{
				Print("[BatchCreatePrefabs] WARNING: Entity at index " + i + " is null, skipping.", LogLevel.WARNING);
				continue;
			}

			string entityName = entSrc.GetName();
			if (entityName == string.Empty)
			{
				Print("[BatchCreatePrefabs] WARNING: Entity at index " + i + " has no name, skipping.", LogLevel.WARNING);
				continue;
			}

			entityNames.Insert(entityName);
			entitySources.Insert(entSrc);
		}

		int succeeded = 0;
		int failed = 0;

		for (int i = 0; i < entityNames.Count(); i++)
		{
			string entityName = entityNames[i];
			IEntitySource entSrc = entitySources[i];
			string absPath = absOutputFolder + entityName + ".et";

			api.BeginEntityAction("Batch Create Prefab: " + entityName);

			// First try saving the scene entity directly — works when the entity has children
			// or is already in your addon's scene layer
			bool result = api.CreateEntityTemplate(entSrc, absPath);

			if (!result)
			{
				// Fall back: spawn a temp entity from the ancestor prefab and save that instead.
				// This handles locked base-game entities with no children.
				BaseContainer ancestor = entSrc.GetAncestor();
				string ancestorPath;
				if (ancestor)
					ancestorPath = ancestor.GetResourceName();

				if (ancestorPath != string.Empty)
				{
					IEntitySource tempSrc = api.CreateEntity(ancestorPath, "", api.GetCurrentEntityLayerId(), null, vector.Zero, vector.Zero);
					if (tempSrc)
					{
						result = api.CreateEntityTemplate(tempSrc, absPath);
						api.DeleteEntity(tempSrc);
					}
				}
			}

			api.EndEntityAction();

			if (result)
			{
				Print("[BatchCreatePrefabs] OK: " + entityName + ".et", LogLevel.NORMAL);
				succeeded++;
			}
			else
			{
				Print("[BatchCreatePrefabs] FAILED: " + entityName + " -> " + absPath, LogLevel.ERROR);
				failed++;
			}
		}

		Print("[BatchCreatePrefabs] Done. Succeeded: " + succeeded + " | Failed: " + failed, LogLevel.NORMAL);

		if (succeeded > 0)
			Print("[BatchCreatePrefabs] Remember to rebuild the resource database (Resource Manager > Rebuild) to register the new prefabs.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	override void Configure()
	{
		Workbench.ScriptDialog("Batch Create Prefabs — Settings", "Set the addon-relative output folder.", this);
	}
}
