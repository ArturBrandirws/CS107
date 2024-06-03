// Include standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

// Include custom libraries
#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h"
#include "hashset.h"

// Define structure for RSS feed items
typedef struct {
  char title[2048];
  char description[2048];
  char url[2048];
  char *activeField;
} rssFeedItem;

// Function prototypes
static void Welcome(const char *welcomeTextURL);
static void BuildIndices(const char *feedsFileURL);
static void ProcessFeed(const char *remoteDocumentURL);
static void PullAllNewsItems(urlconnection *urlconn);
static void ProcessStartTag(void *userData, const char *name, const char **atts);
static void ProcessEndTag(void *userData, const char *name);
static void ProcessTextData(void *userData, const char *text, int len);
static void ParseArticle(const char *articleTitle, const char *articleURL);
static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *articleURL);
static void QueryIndices();
static void ProcessResponse(const char *word);
static bool WordIsWellFormed(const char *word);

// URLs for welcome text and default feeds file
static const char *const kWelcomeTextURL = "http://cs107.stanford.edu/rss-news/welcome.txt";
static const char *const kDefaultStopWordsURL = "http://cs107.stanford.edu/rss-news/stop-words.txt";
static const char *const kDefaultFeedsFileURL = "http://cs107.stanford.edu/rss-news/rss-feeds.txt";

// Main function
int main(int argc, char **argv)
{
  // Set feeds file URL based on command-line arguments
  const char *feedsFileURL = (argc == 1) ? kDefaultFeedsFileURL : argv[1];
  
  // Display welcome text
  Welcome(kWelcomeTextURL);
  // Build indices from feeds file
  BuildIndices(feedsFileURL);
  // Query indices
  QueryIndices();
  return 0;
}

// Delimiters for tokenizing
static const char *const kNewLineDelimiters = "\r\n";

// Display welcome text from URL
static void Welcome(const char *welcomeTextURL)
{
  // Create URL and URL connection
  url u;
  urlconnection urlconn;
  
  // Initialize URL
  URLNewAbsolute(&u, welcomeTextURL);
  // Establish URL connection
  URLConnectionNew(&urlconn, &u);
  
  // Check for redirection
  if (urlconn.responseCode / 100 == 3) {
    Welcome(urlconn.newUrl);
  } else {
    // Create streamtokenizer
    streamtokenizer st;
    char buffer[4096];
    // Initialize streamtokenizer
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    // Read and print each line of the welcome text
    while (STNextToken(&st, buffer, sizeof(buffer))) {
      printf("%s\n", buffer);
    }  
    printf("\n");
    // Dispose of streamtokenizer
    STDispose(&st); 
  }

  // Dispose of URL connection and URL
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

// Number of index entry buckets
static const int kNumIndexEntryBuckets = 10007;

// Build indices from feeds file
static void BuildIndices(const char *feedsFileURL)
{
  // Create URL and URL connection
  url u;
  urlconnection urlconn;
  
  // Initialize URL
  URLNewAbsolute(&u, feedsFileURL);
  // Establish URL connection
  URLConnectionNew(&urlconn, &u);
  
  // Check for redirection
  if (urlconn.responseCode / 100 == 3) {
    BuildIndices(urlconn.newUrl);
  } else {
    // Create streamtokenizer
    streamtokenizer st;
    char remoteDocumentURL[2048];
    
    // Initialize streamtokenizer
    STNew(&st, urlconn.dataStream, kNewLineDelimiters, true);
    // Read each line of the feeds file and process the feed
    while (STSkipUntil(&st, ":") != EOF) {
      STSkipOver(&st, ": "); 
      STNextToken(&st, remoteDocumentURL, sizeof(remoteDocumentURL));   
      ProcessFeed(remoteDocumentURL);
    }
    
    printf("\n");
    // Dispose of streamtokenizer
    STDispose(&st);
  }
  
  // Dispose of URL connection and URL
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

// Process feed from remote document URL
static void ProcessFeed(const char *remoteDocumentURL)
{
  // Create URL and URL connection
  url u;
  urlconnection urlconn;
  
  // Initialize URL
  URLNewAbsolute(&u, remoteDocumentURL);
  // Establish URL connection
  URLConnectionNew(&urlconn, &u);

  // Check response code and handle accordingly
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to "\"%s\".  Ignoring...", u.serverName);
              break;
      case 200: PullAllNewsItems(&urlconn);
                break;
      case 301: 
      case 302: ProcessFeed(urlconn.newUrl);
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		      u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	       break;
  };
  
  // Dispose of URL connection and URL
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

// Pull all news items from URL connection
static void PullAllNewsItems(urlconnection *urlconn)
{
  // Initialize RSS feed item and streamtokenizer
  rssFeedItem item;
  streamtokenizer st;
  char buffer[2048];

  // Create XML parser
  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  // Set user data for XML parser
  XML_SetUserData(rssFeedParser, &item);
  // Set element handler for XML parser
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  // Set character data handler for XML parser
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  // Initialize streamtokenizer
  STNew(&st, urlconn->dataStream, "\n", false);
  // Parse XML data using the XML parser
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  // End parsing and free XML parser
  XML_Parse(rssFeedParser, "", 0, true);
  XML_ParserFree(rssFeedParser);  
}

// Process start tag encountered by XML parser
static void ProcessStartTag(void *userData, const char *name, const char **atts)
{
  // Cast user data to RSS feed item
  rssFeedItem *item = userData;
  // Check tag name and set active field accordingly
  if (strcasecmp(name, "item") == 0) {
    memset(item, 0, sizeof(rssFeedItem));
  } else if (strcasecmp(name, "title") == 0) {
    item->activeField = item->title;
  } else if (strcasecmp(name, "description") == 0) {
    item->activeField = item->description;
  } else if (strcasecmp(name, "link") == 0) {
    item->activeField = item->url;
  }
}

// Process end tag encountered by XML parser
static void ProcessEndTag(void *userData, const char *name)
{
  // Cast user data to RSS feed item
  rssFeedItem *item = userData;
  // Reset active field when encountering end of item tag
  item->activeField = NULL;
  if (strcasecmp(name, "item") == 0)
    // Parse article when encountering end of item tag
    ParseArticle(item->title, item->url);
}

// Process text data encountered by XML parser
static void ProcessTextData(void *userData, const char *text, int len)
{
  // Cast user data to RSS feed item
  rssFeedItem *item = userData;
  // Ignore if active field is not set
  if (item->activeField == NULL) return; 
  char buffer[len + 1];
  // Copy text to buffer and concatenate to active field
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(item->activeField, buffer, 2048);
}

// Parse article from article title and URL
static void ParseArticle(const char *articleTitle, const char *articleURL)
{
  // Create URL and URL connection
  url u;
  urlconnection urlconn;
  streamtokenizer st;
  
  // Initialize URL
  URLNewAbsolute(&u, articleURL);
  // Establish URL connection
  URLConnectionNew(&urlconn, &u);

  // Check response code and handle accordingly
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
              break;
      case 200: printf("[%s] Indexing \"%s\"\n", u.serverName, articleTitle);
	        // Initialize streamtokenizer
	        STNew(&st, urlconn.dataStream, kTextDelimiters, false);
		// Scan article content
		ScanArticle(&st, articleTitle, articleURL);
		STDispose(&st);
		break;
      case 301: 
      case 302: 
	        // Follow redirection
	        ParseArticle(articleTitle, urlconn.newUrl);
		break;
      default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
	       break;
  }
  
  // Dispose of URL connection and URL
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

// Delimiters for scanning article content
static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";

// Scan article content using streamtokenizer
static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *articleURL)
{
  // Initialize word count and buffers
  int numWords = 0;
  char word[1024];
  char longestWord[1024] = {'\0'};

  // Read each word and process
  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); 
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word)) {
	numWords++;
	if (strlen(word) > strlen(longestWord))
	  strcpy(longestWord, word);
      }
    }
  }

  // Print word count and longest word
  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
  printf("\tThe longest word scanned was \"%s\".", longestWord);
  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) 
    printf(" [Ooooo... long word!]");
  printf("\n");
}

// Query indices for search terms
static void QueryIndices()
{
  char response[1024];
  while (true) {
    // Prompt user for query term
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    // Process user response
    ProcessResponse(response);
  }
}

// Process user response for query
static void ProcessResponse(const char *word)
{
  if (WordIsWellFormed(word)) {
    printf("\tWell, we don't have the database mapping words to online news articles yet, but if we DID have\n");
    printf("\tour hashset of indices, we'd list all of the articles containing \"%s\".\n", word);
  } else {
    printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
  }
}

// Check if word is well-formed
static bool WordIsWellFormed(const char *word)
{
  int i;
  // Empty string is considered well-formed
  if (strlen(word) == 0) return true;
  // Check if first character is alphabetic
  if (!isalpha((int) word[0])) return false;
  // Check if remaining characters are alphanumeric or hyphen
  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}


