#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>
#include <pthread.h> 
#include <semaphore.h> 
#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"
#include "vector.h"
#include "hashset.h"

typedef struct {
  hashset stopWords;
  hashset indices;
  vector previouslySeenArticles;
} rssDatabase;

typedef struct {
  char title[2048];
  char url[2048];
  char *activeField; // field that should be populated... 
} rssFeedEntry;

typedef struct {
  rssDatabase *db;
  rssFeedEntry entry;
} rssFeedState;

typedef struct {
  const char *title;
  const char *server;
  const char *fullURL;
} rssNewsArticle;

typedef struct {
  const char *meaningfulWord;
  vector relevantArticles;
} rssIndexEntry;

typedef struct {
  int articleIndex;
  int freq;
} rssRelevantArticleEntry;

typedef struct {
  rssDatabase *db;
  const char *articleTitle;
  const char *articleURL;
} threadArgs;

sem_t semaphore; // Semaphore to limit concurrent connections

static void Welcome(const char *welcomeTextURL);
static void LoadStopWords(hashset *stopWords, const char *stopWordsURL);
static void BuildIndices(rssDatabase *db, const char *feedsFileName);
static void ProcessFeed(rssDatabase *db, const char *remoteDocumentName);
static void PullAllNewsItems(rssDatabase *db, urlconnection *urlconn);

static void ProcessStartTag(void *userData, const char *name, const char **atts);
static void ProcessEndTag(void *userData, const char *name);
static void ProcessTextData(void *userData, const char *text, int len);

static void ParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL);
static void* ThreadedParseArticle(void *args); // Added for threaded article parsing
static void ScanArticle(streamtokenizer *st, int articleID, hashset *indices, hashset *stopWords);
static bool WordIsWorthIndexing(const char *word, hashset *stopWords);
static void AddWordToIndices(hashset *indices, const char *word, int articleIndex);
static void QueryIndices(rssDatabase *db);
static void ProcessResponse(rssDatabase *db, const char *word);
static void ListTopArticles(rssIndexEntry *index, vector *previouslySeenArticles);
static bool WordIsWellFormed(const char *word);
static int StringHash(const void *s, int numBuckets);
static int StringCompare(const void *elem1, const void *elem2);
static void StringFree(void *elem);

int main(int argc, char **argv) {
    const char *feedsFileName = (argc == 1) ? "rss-feeds.txt" : argv[1];
    const char *welcomeTextURL = (argc < 3) ? "http://cs107.stanford.edu/readings/welcome.txt" : argv[2];
    const char *stopWordsURL = (argc < 4) ? "http://cs107.stanford.edu/readings/stop-words.txt" : argv[3];

    rssDatabase db;
    HashSetNew(&db.stopWords, sizeof(char *), 1009, StringHash, StringCompare, StringFree);
    HashSetNew(&db.indices, sizeof(rssIndexEntry), 10007, StringHash, StringCompare, StringFree);
    VectorNew(&db.previouslySeenArticles, sizeof(char *), StringFree);

    Welcome(welcomeTextURL);
    LoadStopWords(&db.stopWords, stopWordsURL);
    BuildIndices(&db, feedsFileName);
    QueryIndices(&db);

    HashSetDispose(&db.stopWords);
    HashSetDispose(&db.indices);
    VectorDispose(&db.previouslySeenArticles);

    return 0;
}

static void Welcome(const char *welcomeTextURL) {
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, welcomeTextURL);
  URLConnectionNew(&urlconn, &u);
  
  if (urlconn.responseCode / 100 == 3) {
    Welcome(urlconn.newUrl);
  } else {
    streamtokenizer st;
    char buffer[4096];
    STNew(&st, urlconn.dataStream, "\r\n", true);
    while (STNextToken(&st, buffer, sizeof(buffer))) {
      printf("%s\n", buffer);
    }
    printf("\n");
    STDispose(&st);
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

static void LoadStopWords(hashset *stopWords, const char *stopWordsURL) {
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, stopWordsURL);
  URLConnectionNew(&urlconn, &u);
  
  if (urlconn.responseCode / 100 == 3) {
    LoadStopWords(stopWords, urlconn.newUrl);
  } else {
    streamtokenizer st;
    char buffer[4096];
    HashSetNew(stopWords, sizeof(char *), 1009, StringHash, StringCompare, StringFree);
    STNew(&st, urlconn.dataStream, "\r\n", true);
    while (STNextToken(&st, buffer, sizeof(buffer))) {
      char *stopWord = strdup(buffer);
      HashSetEnter(stopWords, &stopWord);
    }
    STDispose(&st);
  }

  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

static void BuildIndices(rssDatabase *db, const char *feedsFileName) {
  url u;
  urlconnection urlconn;
  
  URLNewAbsolute(&u, feedsFileName);
  URLConnectionNew(&urlconn, &u);
  PullAllNewsItems(db, &urlconn);
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

// Function to process the RSS feed
static void ProcessFeed(rssDatabase *db, const char *remoteDocumentName) {
    // Declare a variable to hold the URL connection
    urlconnection urlconn;
    // Initialize the URL connection with the remote document name
    URLConnectionNew(&urlconn, remoteDocumentName);
    // Pull all news items from the URL connection and store them in the database
    PullAllNewsItems(db, &urlconn);
    // Dispose of the URL connection
    URLConnectionDispose(&urlconn);
}

static void PullAllNewsItems(rssDatabase *db, urlconnection *urlconn) {
  rssFeedState state = {db}; // passed through the parser by address as auxiliary data.
  streamtokenizer st;
  char buffer[2048];

  XML_Parser rssFeedParser = XML_ParserCreate(NULL);
  XML_SetUserData(rssFeedParser, &state);
  XML_SetElementHandler(rssFeedParser, ProcessStartTag, ProcessEndTag);
  XML_SetCharacterDataHandler(rssFeedParser, ProcessTextData);

  STNew(&st, urlconn->dataStream, "\n", false);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    XML_Parse(rssFeedParser, buffer, strlen(buffer), false);
  }
  STDispose(&st);
  
  XML_Parse(rssFeedParser, "", 0, true); // instructs the xml parser that we're done parsing..
  XML_ParserFree(rssFeedParser);
}

static void ProcessStartTag(void *userData, const char *name, const char **atts) {
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (strcasecmp(name, "item") == 0) {
    memset(entry, 0, sizeof(rssFeedEntry));
  } else if (strcasecmp(name, "title") == 0) {
    entry->activeField = entry->title;
  } else if (strcasecmp(name, "link") == 0) {
    entry->activeField = entry->url;
  }
}

static void ProcessEndTag(void *userData, const char *name) {
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  entry->activeField = NULL;
  if (strcasecmp(name, "item") == 0) {
    articleTask *task = malloc(sizeof(articleTask));
    task->db = state->db;
    strcpy(task->articleTitle, entry->title);
    strcpy(task->articleURL, entry->url);

    pthread_t thread;
    pthread_create(&thread, NULL, DownloadAndParseArticle, task);
    pthread_detach(thread);
  }
}

static void ProcessTextData(void *userData, const char *text, int len) {
  rssFeedState *state = userData;
  rssFeedEntry *entry = &state->entry;
  if (entry->activeField == NULL) return;
  char buffer[len + 1];
  memcpy(buffer, text, len);
  buffer[len] = '\0';
  strncat(entry->activeField, buffer, 2048);
}

static void ParseArticle(rssDatabase *db, const char *articleTitle, const char *articleURL) {
  sem_wait(&semaphore);
  sem_wait(&mutex);

  url u;
  urlconnection urlconn;
  streamtokenizer st;
  int articleID;
  
  URLNewAbsolute(&u, articleURL);
  rssNewsArticle newsArticle = { articleTitle, u.serverName, u.fullName };
  if (VectorSearch(&db->previouslySeenArticles, &newsArticle, NewsArticleCompare, 0, false) >= 0) {
    printf("[Ignoring \"%s\": we've seen it before.]\n", articleTitle);
    URLDispose(&u);
    sem_post(&mutex);
    sem_post(&semaphore);
    return;
  }

  // function for threaded article parsing
  // Define the function for threaded article parsing
  static void* ThreadedParseArticle(void *args) { 
    // Cast the input argument to threadArgs type
    threadArgs *tArgs = (threadArgs *) args;
    // Extract the database pointer from thread arguments
    rssDatabase *db = tArgs->db;
    // Extract the article title from thread arguments
    const char *articleTitle = tArgs->articleTitle;
    // Extract the article URL from thread arguments
    const char *articleURL = tArgs->articleURL;

    // Parse the article using the provided database, title, and URL
    ParseArticle(db, articleTitle, articleURL);

    // Free the allocated memory for thread arguments
    free(tArgs); 
    // Release the semaphore
    sem_post(&semaphore); 
    // Return NULL to indicate the end of the thread
    return NULL;
  }
  
  URLConnectionNew(&urlconn, &u);
  switch (urlconn.responseCode) {
    case 0: printf("Unable to connect to \"%s\". Domain name or IP address is nonexistent.\n", articleURL); break;
    case 200: 
      printf("[%s] Indexing \"%s\"\n", u.serverName, articleTitle);
      NewsArticleClone(&newsArticle, articleTitle, u.serverName, u.fullName);
      VectorAppend(&db->previouslySeenArticles, &newsArticle);
      articleID = VectorLength(&db->previouslySeenArticles) - 1;
      STNew(&st, urlconn.dataStream, " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`", false);
      ScanArticle(&st, articleID, &db->indices, &db->stopWords);
      STDispose(&st);
      break;
    case 301:
    case 302: 
      ParseArticle(db, articleTitle, urlconn.newUrl); 
      break;
    default: 
      printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
      break;
  }

  URLConnectionDispose(&urlconn);
  URLDispose(&u);
  
  sem_post(&mutex);
  sem_post(&semaphore);
}

static void ScanArticle(streamtokenizer *st, int articleID, hashset *indices, hashset *stopWords) {
  char word[1024];

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st);
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWorthIndexing(word, stopWords))
        AddWordToIndices(indices, word, articleID);
    }
  }
}

static bool WordIsWorthIndexing(const char *word, hashset *stopWords) {
  return WordIsWellFormed(word) && HashSetLookup(stopWords, &word) == NULL;
}

static void AddWordToIndices(hashset *indices, const char *word, int articleIndex) {
  rssIndexEntry indexEntry = { word }; // partial initialization
  rssIndexEntry *existingIndexEntry = HashSetLookup(indices, &indexEntry);
  if (existingIndexEntry == NULL) {
    indexEntry.meaningfulWord = strdup(word);
    VectorNew(&indexEntry.relevantArticles, sizeof(rssRelevantArticleEntry), NULL, 0);
    HashSetEnter(indices, &indexEntry);
    existingIndexEntry = HashSetLookup(indices, &indexEntry); // pretend like it's been there all along
    assert(existingIndexEntry != NULL);
  }

  rssRelevantArticleEntry articleEntry = { articleIndex, 0 };
  int existingArticleIndex =
    VectorSearch(&existingIndexEntry->relevantArticles, &articleEntry, ArticleIndexCompare, 0, false);
  if (existingArticleIndex == -1) {
    VectorAppend(&existingIndexEntry->relevantArticles, &articleEntry);
    existingArticleIndex = VectorLength(&existingIndexEntry->relevantArticles) - 1;
  }
  
  rssRelevantArticleEntry *existingArticleEntry = 
    VectorNth(&existingIndexEntry->relevantArticles, existingArticleIndex);
  existingArticleEntry->freq++;
}

static void QueryIndices(rssDatabase *db) {
  char response[1024];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(db, response);
  }
  
  HashSetDispose(&db->indices);
  VectorDispose(&db->previouslySeenArticles); 
  HashSetDispose(&db->stopWords);
}

static void ProcessResponse(rssDatabase *db, const char *word) {
  if (!WordIsWellFormed(word)) {
    printf("That search term couldn't possibly be in our set of indices.\n\n");
    return;
  }
  
  if (HashSetLookup(&db->stopWords, &word) != NULL) {
    printf("\"%s\" is too common a word to be taken seriously. Please be more specific.\n\n", word);
    return;
  }

  rssIndexEntry entry = { word };
  rssIndexEntry *existingIndex = HashSetLookup(&db->indices, &entry);
  if (existingIndex == NULL) {
    printf("None of today's news articles contain the word \"%s\".\n\n", word);
    return;
  }

  ListTopArticles(existingIndex, &db->previouslySeenArticles);
}

static void ListTopArticles(rssIndexEntry *matchingEntry, vector *previouslySeenArticles) {
  int i, numArticles, articleIndex, count;
  rssRelevantArticleEntry *relevantArticleEntry;
  rssNewsArticle *relevantArticle;
  
  numArticles = VectorLength(&matchingEntry->relevantArticles);
  printf("Nice! We found %d article%s that include%s the word \"%s\". ", 
         numArticles, (numArticles == 1) ? "" : "s", (numArticles != 1) ? "" : "s", matchingEntry->meaningfulWord);
  if (numArticles > 10) { printf("[We'll just list 10 of them, though.]"); numArticles = 10; }
  printf("\n\n");
  
  VectorSort(&matchingEntry->relevantArticles, ArticleFrequencyCompare);
  for (i = 0; i < numArticles; i++) {
    relevantArticleEntry = VectorNth(&matchingEntry->relevantArticles, i);
    articleIndex = relevantArticleEntry->articleIndex;
    count = relevantArticleEntry->freq;
    relevantArticle = VectorNth(previouslySeenArticles, articleIndex);
    printf("\t%2d.) \"%s\" [search term occurs %d time%s]\n", i + 1, 
           relevantArticle->title, count, (count == 1) ? "" : "s");
    printf("\t%2s   \"%s\"\n", "", relevantArticle->fullURL);
  }
  
  printf("\n");
}

static bool WordIsWellFormed(const char *word) {
  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  for (int i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false; 

  return true;
}

static int StringHash(const void *elem, int numBuckets) {
  unsigned long hashcode = 0;
  const char *s = *(const char **) elem;

  for (int i = 0; i < strlen(s); i++)
    hashcode = hashcode * -1664117991L + tolower(s[i]);

  return hashcode % numBuckets;
}

static int StringCompare(const void *elem1, const void *elem2) {
  const char *s1 = *(const char **) elem1;
  const char *s2 = *(const char **) elem2;
  return strcasecmp(s1, s2);
}

static void StringFree(void *elem) {
  free(*(char **)elem);
}

static void NewsArticleClone(rssNewsArticle *article, const char *title, 
                             const char *server, const char *fullURL) {
  article->title = strdup(title);
  article->server = strdup(server);
  article->fullURL = strdup(fullURL);
}

static int NewsArticleCompare(const void *elem1, const void *elem2) {
  const rssNewsArticle *article1 = elem1;
  const rssNewsArticle *article2 = elem2;
  if ((StringCompare(&article1->title, &article2->title) == 0) &&
      (StringCompare(&article1->server, &article2->server) == 0)) return 0;
  
  return StringCompare(&article1->fullURL, &article2->fullURL);
}

static void NewsArticleFree(void *elem) {
  rssNewsArticle *article = elem;
  StringFree(&article->title);
  StringFree(&article->server);
  StringFree(&article->fullURL);
}

static int IndexEntryHash(const void *elem, int numBuckets) {
  const rssIndexEntry *entry = elem;
  return StringHash(&entry->meaningfulWord, numBuckets);
}

static int IndexEntryCompare(const void *elem1, const void *elem2) {
  const rssIndexEntry *entry1 = elem1;
  const rssIndexEntry *entry2 = elem2;
  return StringCompare(&entry1->meaningfulWord, &entry2->meaningfulWord);
}

static void IndexEntryFree(void *elem) {
  rssIndexEntry *entry = elem;
  StringFree(&entry->meaningfulWord);
  VectorDispose(&entry->relevantArticles);
}

static int ArticleIndexCompare(const void *elem1, const void *elem2) {
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry1->articleIndex - entry2->articleIndex;
}

static int ArticleFrequencyCompare(const void *elem1, const void *elem2) {
  const rssRelevantArticleEntry *entry1 = elem1;
  const rssRelevantArticleEntry *entry2 = elem2;
  return entry2->freq - entry1->freq;
}
