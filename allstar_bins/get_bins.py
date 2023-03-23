import requests
from bs4 import BeautifulSoup

# URL of the page to retrieve
url = "https://allstar.jhuapl.edu"

# Send a GET request to the URL to retrieve the page content
response = requests.get(url)

# Check that the response was successful
if response.status_code == 200:
    # Parse the HTML content of the page using BeautifulSoup
    soup = BeautifulSoup(response.content, "html.parser")
    
    # Find all links on the page
    links = soup.find_all("a", href=True)
    
    # Loop through the links and check if they have additional links on their own pages
    for link in links:
        # Get the URL of the linked page
        linked_url = link["href"]
        
        # Send a GET request to the linked URL to retrieve the page content
        linked_response = requests.get(linked_url)
        
        # Check that the response was successful
        if linked_response.status_code == 200:
            # Parse the HTML content of the linked page using BeautifulSoup
            linked_soup = BeautifulSoup(linked_response.content, "html.parser")
            
            # Find all links on the linked page
            linked_links = linked_soup.find_all("a", href=True)
            
            # Check if the linked page has any links
            if len(linked_links) > 0:
                print(f"Page {linked_url} has {len(linked_links)} additional links")
        else:
            print(f"Error retrieving page {linked_url}: {linked_response.status_code}")
else:
    print(f"Error retrieving page {url}: {response.status_code}")
