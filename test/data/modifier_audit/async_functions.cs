using System;
using System.Threading.Tasks;

class Example {
    public async Task<string> FetchData(string url) {
        return await HttpClient.GetStringAsync(url);
    }

    public void SyncHelper() {
        Console.WriteLine("sync");
    }

    public static void StaticHelper() {
        Console.WriteLine("static");
    }

    private async Task PrivateAsync() {
        await Task.Delay(1000);
    }

    protected virtual void VirtualMethod() {
        Console.WriteLine("virtual");
    }
}
