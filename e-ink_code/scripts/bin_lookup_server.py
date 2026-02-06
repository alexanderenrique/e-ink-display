#!/usr/bin/env python3
"""
Bin Lookup Server for ESP32 Shelf Labels

This server pre-processes user and bin data from NEMO API and provides
a lightweight endpoint for ESP32 devices to query bin owner information.

Usage:
    python bin_lookup_server.py

The server will:
1. Fetch all users from NEMO_USER_URL
2. Fetch all bins from NEMO_BIN_URL (recurring_consumable_charges endpoint)
3. Build a lookup table mapping bin IDs to user info (using customer field)
4. Serve GET /bin/<bin_id> endpoint that returns JSON with owner info

The recurring_consumable_charges API returns bins with this structure:
    {
        "id": 317,
        "name": "Bin E01",
        "quantity": 1,
        "customer": 447,  # This is the user ID
        "consumable": 64,
        "project": 869,
        ...
    }

Environment variables (from .env):
    NEMO_API_KEY: API token for NEMO authentication
    NEMO_USER_URL: URL to fetch users from
    NEMO_BIN_URL: URL to fetch bins from (recurring_consumable_charges)
"""

import os
import sys
import json
import time
from typing import Dict, Optional, Any
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import requests
from dotenv import load_dotenv

# Load environment variables from .env file
load_dotenv()

# Configuration from environment
NEMO_API_KEY = os.getenv('NEMO_API_KEY')
NEMO_USER_URL = os.getenv('NEMO_USER_URL', 'https://nemo.stanford.edu/api/users/')
NEMO_BIN_URL = os.getenv('NEMO_BIN_URL', 'https://nemo.stanford.edu/api/recurring_consumable_charges/')
SERVER_HOST = os.getenv('SERVER_HOST', '0.0.0.0')
SERVER_PORT = int(os.getenv('SERVER_PORT', '8080'))
CACHE_REFRESH_INTERVAL = int(os.getenv('CACHE_REFRESH_INTERVAL', '3600'))  # 1 hour default
# API timeout in seconds (None = no timeout, useful for slow APIs)
API_TIMEOUT = int(os.getenv('API_TIMEOUT', '300')) if os.getenv('API_TIMEOUT') else None  # Default: 5 minutes or no timeout

# Global cache
user_lookup: Dict[int, Dict[str, Any]] = {}
bin_lookup: Dict[str, Dict[str, Any]] = {}
last_refresh_time = 0


def fetch_all_users() -> Dict[int, Dict[str, Any]]:
    """Fetch all users from NEMO API and build lookup table."""
    print(f"Fetching users from {NEMO_USER_URL}...")
    
    headers = {
        'Authorization': f'Token {NEMO_API_KEY}',
        'Content-Type': 'application/json'
    }
    
    users = {}
    url = NEMO_USER_URL
    
    # Handle pagination if API supports it
    while url:
        try:
            response = requests.get(url, headers=headers, timeout=API_TIMEOUT)
            response.raise_for_status()
            
            data = response.json()
            
            # Handle both array and paginated responses
            if isinstance(data, list):
                user_list = data
                url = None  # No pagination
            elif isinstance(data, dict) and 'results' in data:
                user_list = data['results']
                url = data.get('next')  # Pagination URL
            else:
                print(f"Unexpected response format: {type(data)}")
                break
            
            for user in user_list:
                user_id = user.get('id')
                if user_id:
                    users[user_id] = {
                        'id': user_id,
                        'username': user.get('username', ''),
                        'first_name': user.get('first_name', ''),
                        'last_name': user.get('last_name', ''),
                        'email': user.get('email', ''),
                    }
            
            print(f"Loaded {len(users)} users so far...")
            
        except requests.exceptions.RequestException as e:
            print(f"Error fetching users: {e}")
            break
    
    print(f"Total users loaded: {len(users)}")
    return users


def fetch_all_bins() -> Dict[str, Dict[str, Any]]:
    """Fetch all bins from NEMO API and build lookup table."""
    print(f"Fetching bins from {NEMO_BIN_URL}...")
    
    headers = {
        'Authorization': f'Token {NEMO_API_KEY}',
        'Content-Type': 'application/json'
    }
    
    bins = {}
    url = NEMO_BIN_URL
    
    # Handle pagination if API supports it
    while url:
        try:
            response = requests.get(url, headers=headers, timeout=API_TIMEOUT)
            response.raise_for_status()
            
            data = response.json()
            
            # Handle both array and paginated responses
            if isinstance(data, list):
                bin_list = data
                url = None  # No pagination
            elif isinstance(data, dict) and 'results' in data:
                bin_list = data['results']
                url = data.get('next')  # Pagination URL
            else:
                print(f"Unexpected response format: {type(data)}")
                break
            
            for bin_data in bin_list:
                # Store bin by both ID and name for flexible lookup
                if 'id' in bin_data:
                    bin_id_str = str(bin_data['id'])
                    bins[bin_id_str] = bin_data
                
                # Also store by name if it exists and is different from ID
                if 'name' in bin_data:
                    bin_name = bin_data['name']
                    if bin_name and bin_name != str(bin_data.get('id', '')):
                        bins[bin_name] = bin_data
            
            print(f"Loaded {len(bins)} bins so far...")
            
        except requests.exceptions.RequestException as e:
            print(f"Error fetching bins: {e}")
            break
    
    print(f"Total bins loaded: {len(bins)}")
    return bins


def refresh_cache():
    """Refresh the user and bin lookup caches."""
    global user_lookup, bin_lookup, last_refresh_time
    
    print("Refreshing cache...")
    start_time = time.time()
    
    user_lookup = fetch_all_users()
    bin_lookup = fetch_all_bins()
    
    last_refresh_time = time.time()
    elapsed = last_refresh_time - start_time
    
    print(f"Cache refresh complete in {elapsed:.2f} seconds")
    print(f"Users: {len(user_lookup)}, Bins: {len(bin_lookup)}")


def get_bin_info(bin_id: str) -> Optional[Dict[str, Any]]:
    """Get bin information including owner details."""
    # Check if cache needs refresh
    global last_refresh_time
    if time.time() - last_refresh_time > CACHE_REFRESH_INTERVAL:
        refresh_cache()
    
    # Look up bin
    bin_data = bin_lookup.get(bin_id)
    if not bin_data:
        return None
    
    # Extract customer ID (customer field contains the user ID)
    # Handle different formats: int, string, or nested object
    customer_id = None
    customer_field = bin_data.get('customer')
    
    if isinstance(customer_field, int):
        customer_id = customer_field
    elif isinstance(customer_field, str):
        try:
            customer_id = int(customer_field)
        except ValueError:
            pass
    elif isinstance(customer_field, dict):
        customer_id = customer_field.get('id')
    
    # Look up user info using customer ID
    user_info = None
    if customer_id:
        user_info = user_lookup.get(customer_id)
    
    # Build response - only include what ESP32 needs for display
    result = {
        'bin_id': bin_id,
        'bin_name': bin_data.get('name', ''),
    }
    
    if user_info:
        # Build full name
        name_parts = []
        if user_info.get('first_name'):
            name_parts.append(user_info['first_name'])
        if user_info.get('last_name'):
            name_parts.append(user_info['last_name'])
        
        result['owner'] = {
            'name': ' '.join(name_parts) if name_parts else user_info.get('username', 'Unknown'),
            'username': user_info.get('username', ''),
            'email': user_info.get('email', ''),
        }
    else:
        result['owner'] = None
    
    return result


class BinLookupHandler(BaseHTTPRequestHandler):
    """HTTP request handler for bin lookup API."""
    
    def do_GET(self):
        """Handle GET requests."""
        parsed_path = urlparse(self.path)
        
        # Handle CORS preflight
        if self.path == '/bin' or parsed_path.path == '/bin':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'error': 'Please specify a bin ID: /bin/<bin_id>'
            }).encode())
            return
        
        # Handle /bin/<bin_id>
        if parsed_path.path.startswith('/bin/'):
            bin_id = parsed_path.path[5:]  # Remove '/bin/'
            
            if not bin_id:
                self.send_error(400, "Bin ID required")
                return
            
            bin_info = get_bin_info(bin_id)
            
            if bin_info is None:
                self.send_response(404)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({
                    'error': f'Bin not found: {bin_id}'
                }).encode())
                return
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(bin_info).encode())
            return
        
        # Handle /refresh endpoint to manually refresh cache
        if parsed_path.path == '/refresh':
            refresh_cache()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'status': 'Cache refreshed',
                'users': len(user_lookup),
                'bins': len(bin_lookup)
            }).encode())
            return
        
        # Handle /health endpoint
        if parsed_path.path == '/health':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'status': 'ok',
                'users': len(user_lookup),
                'bins': len(bin_lookup),
                'last_refresh': last_refresh_time
            }).encode())
            return
        
        # 404 for unknown paths
        self.send_error(404, "Not found")
    
    def log_message(self, format, *args):
        """Override to use print instead of stderr."""
        print(f"[{self.address_string()}] {format % args}")


def main():
    """Main server function."""
    if not NEMO_API_KEY:
        print("Error: NEMO_API_KEY not set in environment variables")
        sys.exit(1)
    
    print("=" * 60)
    print("Bin Lookup Server for ESP32 Shelf Labels")
    print("=" * 60)
    print(f"Server: {SERVER_HOST}:{SERVER_PORT}")
    print(f"NEMO User URL: {NEMO_USER_URL}")
    print(f"NEMO Bin URL: {NEMO_BIN_URL}")
    print(f"Cache refresh interval: {CACHE_REFRESH_INTERVAL} seconds")
    print(f"API timeout: {API_TIMEOUT if API_TIMEOUT else 'None (no timeout)'}")
    print("=" * 60)
    
    # Initial cache load
    refresh_cache()
    
    # Start HTTP server
    server = HTTPServer((SERVER_HOST, SERVER_PORT), BinLookupHandler)
    print(f"\nServer started on http://{SERVER_HOST}:{SERVER_PORT}")
    print("Endpoints:")
    print("  GET /bin/<bin_id>  - Get bin owner information")
    print("  GET /refresh        - Manually refresh cache")
    print("  GET /health         - Health check")
    print("\nPress Ctrl+C to stop the server\n")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        server.shutdown()


if __name__ == '__main__':
    main()
