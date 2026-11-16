namespace HNKC.AIBox.Services;

public class ImageTest(IFileService fileService)
{
    public async Task<string> getImage(string id)
    {
        try
        {
            var stream = fileService.Download($"files/{id}.png");
            using (var memoryStream = new MemoryStream())
            {
                await stream.CopyToAsync(memoryStream);
                var bytes = memoryStream.ToArray();
                return "data:image/png;base64,"+Convert.ToBase64String(bytes);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine(e);
            throw;
        }

        return "";
    }
}