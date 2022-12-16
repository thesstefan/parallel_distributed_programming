namespace HttpCrawler.Crawler;
public interface IHttpCrawler
{
   public void Run(IList<string> hostNames);
}