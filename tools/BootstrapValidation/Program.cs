using System.Buffers.Binary;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

static void Require(bool a_condition, string a_message)
{
	if (!a_condition)
		throw new InvalidDataException(a_message);
}

static void Validate(ISkyrimModGetter a_mod)
{
	Require(a_mod.ModKey == ModKey.FromFileName("SkyUI_SE.esp"), "Wrong compatibility filename");
	Require(a_mod.ModHeader.MasterReferences.Count == 1 &&
		a_mod.ModHeader.MasterReferences[0].Master == ModKey.FromFileName("Skyrim.esm"), "Wrong master");
	Require(a_mod.EnumerateMajorRecords().Count() == 1, "Unexpected bootstrap records");
	var quest = a_mod.Quests.Single();
	var manager = FormKey.Factory("000802:SkyUI_SE.esp");
	Require(quest.FormKey == manager && quest.EditorID == "SKI_ConfigManagerInstance", "Wrong manager identity");
	Require((ushort)quest.Flags == 0x111, "Wrong startup flags");
	Require(quest.Stages.Count == 0 && quest.Objectives.Count == 0, "Unexpected stage or objective");
	Require(quest.NextAliasID == 1, "Wrong alias counter");
	var alias = quest.Aliases.Single();
	Require(alias.ID == 0 && alias.Name == "PlayerRef", "Wrong player alias");
	Require(alias.ForcedReference.FormKey == FormKey.Factory("000014:Skyrim.esm"), "Wrong player reference");
	var adapter = quest.VirtualMachineAdapter ?? throw new InvalidDataException("Missing VMAD");
	Require(adapter.Version == 5 && adapter.ObjectFormat == 2, "Wrong VMAD version");
	Require(adapter.Scripts.Single().Name == "SKI_ConfigManager", "Wrong manager script");
	Require(adapter.Scripts.Single().Properties.Count == 0, "Unexpected manager properties");
	Require(adapter.Fragments.Count == 0 && string.IsNullOrEmpty(adapter.FileName), "Unexpected quest fragment");
	var load = adapter.Aliases.Single();
	Require(load.Property.Object.FormKey == manager && load.Property.Alias == 0, "Wrong VMAD alias reference");
	Require(load.Version == 5 && load.ObjectFormat == 2, "Wrong alias VMAD version");
	Require(load.Scripts.Single().Name == "SKI_PlayerLoadGameAlias", "Wrong load script");
	Require(load.Scripts.Single().Properties.Count == 0, "Unexpected alias properties");
}

if (args.Length != 1)
	throw new ArgumentException("Usage: BootstrapValidation <SkyUI_SE.esp>");

var path = Path.GetFullPath(args[0]);
var mod = SkyrimMod.CreateFromBinary(path, SkyrimRelease.SkyrimSE);
Validate(mod);
using var overlay = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
Validate(overlay);
var sequence = File.ReadAllBytes(Path.Combine(Path.GetDirectoryName(path)!, "Seq", "SkyUI_SE.seq"));
Require(sequence.Length == 4 && BinaryPrimitives.ReadUInt32LittleEndian(sequence) == 0x01000802,
	"Wrong start-enabled quest sequence");
Console.WriteLine("Mutagen 0.51.3: mutable and overlay readers verified manager, player alias, VMAD and SEQ.");
