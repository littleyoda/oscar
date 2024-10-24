#!/usr/bin/env python3

#    Copyright 2022 github.com/nielm
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

import argparse
from datetime import timedelta
import json
from datetime import date
import logging
import json

from garminconnect import (
    Garmin,
    GarminConnectAuthenticationError,
    GarminConnectConnectionError,
    GarminConnectTooManyRequestsError
)

api = None

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def init_api(email, password):
    """Initialize Garmin API with your credentials."""
    try:
        api = Garmin(email, password)
        api.login()

    except (
        GarminConnectConnectionError,
        GarminConnectAuthenticationError,
        GarminConnectTooManyRequestsError
    ) as err:
        logger.error("Error occurred during Garmin Connect communication: %s", err)
        return None

    return api


def getData(day, email, password):
    global api

    # Init API
    if not api:
        print("Login")
        api = init_api(email, password)
    data = {}
    data["sleep"] = api.get_sleep_data(day.isoformat())
    data["spo2"] = api.get_spo2_data(day.isoformat())
    data["heartrates"] = api.get_heart_rates(day.isoformat())
    return data


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("email", help="e-mail used for https://connect.garmin.com/")
    parser.add_argument("password", help="password")
    args = parser.parse_args()
    print(args.email)
    print(args.password)
    for d in range(0,20):
         print(f"Requesting Day {-d}")
         day = date.today() + timedelta(days=-d)
         fname = f"data-{day}.json"
         data = getData(day, args.email, args.password)
         with open(fname, 'w', encoding='utf-8') as f:
             json.dump(data, f, ensure_ascii=False, indent=4)