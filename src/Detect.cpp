/*******************************************************************************
 *   Copyright 2013-2014 EPFL                                                   *
 *   Copyright 2013-2014 Quentin Bonnard                                        *
 *   Copyright 2013-2014 Ayberk Özgür                                           *
 *                                                                              *
 *   This file is part of chilitags.                                            *
 *                                                                              *
 *   Chilitags is free software: you can redistribute it and/or modify          *
 *   it under the terms of the Lesser GNU General Public License as             *
 *   published by the Free Software Foundation, either version 3 of the         *
 *   License, or (at your option) any later version.                            *
 *                                                                              *
 *   Chilitags is distributed in the hope that it will be useful,               *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *   GNU Lesser General Public License for more details.                        *
 *                                                                              *
 *   You should have received a copy of the GNU Lesser General Public License   *
 *   along with Chilitags.  If not, see <http://www.gnu.org/licenses/>.         *
 *******************************************************************************/

#include "Detect.hpp"

namespace chilitags{

Detect::Detect() :
    mRefineCorners(true),
    mFindQuads(),
    mRefine(),
    mReadBits(),
    mDecode(),
    mFrame(),
    mTags(),
    mBackgroundThread(),
    mBackgroundRunning(false),
    mNeedFrame(true),
    mFrameDelivered(false)
{
}

void Detect::setMinInputWidth(int minWidth)
{
    mFindQuads.setMinInputWidth(minWidth);
}

void Detect::setCornerRefinement(bool refineCorners)
{
    mRefineCorners = refineCorners;
}

void Detect::launchBackgroundThread(Track& track)
{
    if(!mBackgroundRunning){
        mTrack = &track;
        mBackgroundShouldRun = true;
        mBackgroundRunning = true;
        if(pthread_create(&mBackgroundThread, NULL, dispatchRun, (void*)this)){
            mBackgroundShouldRun = false;
            mBackgroundRunning = false;
            //TODO: Alarm that background thread could not be created
        }
    }
}

void Detect::operator()(cv::Mat const& greyscaleImage, TagCornerMap& tags)
{

    //Run single threaded
    if(!mBackgroundRunning){
        mFrame = greyscaleImage;
        mTags = tags;
        doDetection();
        tags = mTags; //TODO: We can do better than copy back here
    }

    //Detection thread running in the background, just deliver the frames and tags
    else{
        if(mNeedFrame){
            pthread_mutex_lock(&inputLock);
            greyscaleImage.copyTo(mFrame);
            mTags = tags; //TODO: Do we really need to deliver tags here?
            mFrameDelivered = true;
            pthread_mutex_unlock(&inputLock);
        }
    }
}

void* Detect::dispatchRun(void* args)
{
    static_cast<Detect*>(args)->run();
    return NULL;
}

void Detect::run()
{
    while(mBackgroundShouldRun){
        while(!mFrameDelivered); //TODO: Replace this disgusting thing with a wait condition

        pthread_mutex_lock(&inputLock);
        mNeedFrame = false;
        doDetection();
        mTrack->update(mTags);
        mNeedFrame = true;
        mFrameDelivered = false;
        pthread_mutex_unlock(&inputLock);
    }
    mBackgroundRunning = false;
}

void Detect::doDetection()
{
    if(mRefineCorners){
        for(const auto& quad : mFindQuads(mFrame)){
            auto refinedQuad = mRefine(mFrame, quad, 1.5/10.);
            auto tag = mDecode(mReadBits(mFrame, refinedQuad), refinedQuad);
            if(tag.first != Decode::INVALID_TAG)
                mTags[tag.first] = tag.second;
            else{
                tag = mDecode(mReadBits(mFrame, quad), quad);
                if(tag.first != Decode::INVALID_TAG)
                    mTags[tag.first] = tag.second;
            }
        }
    }
    else{
        for(const auto& quad : mFindQuads(mFrame)){
            auto tag = mDecode(mReadBits(mFrame, quad), quad);
            if(tag.first != Decode::INVALID_TAG)
                mTags[tag.first] = tag.second;
        }
    }
}

} /* namespace chilitags */

