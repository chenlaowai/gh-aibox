namespace HNKC.AIBox.Files.Common.Storage;

public interface IStorage : IDisk
{
    IDisk Disk();
    IDisk Disk(string diskName);
}