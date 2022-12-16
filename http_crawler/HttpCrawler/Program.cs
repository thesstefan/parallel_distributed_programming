using HttpCrawler.Crawler;

namespace HttpCrawler;

internal static class Program
{
    private static void Main()
    {
        var urls = new List<string> {
            "www.cs.ubbcluj.ro/~hfpop/teaching/pfl/requirements.html",
            "www.cs.ubbcluj.ro/~rlupsa/edu/pdp/index.html",
            "www.cs.ubbcluj.ro/~forest/newton/index.html",
            "www.columbia.edu/~fdc/sample.html"
        };

        IHttpCrawler crawler = new AsyncTaskHttpCrawler();
        crawler.Run(urls);
    }
}