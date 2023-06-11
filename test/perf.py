##########################################################################################################################
########################### TESTS DE PERFORMANCE - G1E3 - THREADS EN ESPACE UTILISATEUR ##################################
##########################################################################################################################

import matplotlib.pyplot as plt
import subprocess
import time
import glob
import os
import numpy as np
import matplotlib.gridspec as gridspec
import random as rand

folder="./install/bin"                                                  ## Dossier d'installation des tests
pthreadsFolder=folder+"/pthreads"                                       ## Dossier d'installation des tests pthread
stackFolder=folder+"/stack"                                             ## Dossier d'installation des tests avec SOH (Stack Overflow Handling)
fichiersTest=glob.glob(os.path.join(folder,"*"))                        ## Ensemble des fichiers de tests

try:
    fichiersTest.remove(pthreadsFolder)                                 ## Supprime de la liste fichiersTest le dossier contenant les tests pthread
except: 
    raise Exception('Pthreads folder not found : use "make pthreads"')

try:
    fichiersTest.remove(stackFolder)                                    ## Supprime de la liste fichiersTest le dossier contenant les tests SOH
except: 
    raise Exception('Stack folder not found : use "make stack"')

fichiersTestPthreads=glob.glob(os.path.join(pthreadsFolder,"*"))        ## Ensemble des fichiers de tests pthread
fichiersTestStack=glob.glob(os.path.join(stackFolder,"*"))           ## Ensemble des fichiers de test SOH

testWithArguments=[21,22,23,31,32,33,51,61,62,71]                       ## Tests avec arguments
nbOfArg=[1,1,1,2,2,2,1,1,1,1]                                           ## Nombre d'arguments pour chaque test avec argument
testNumbers=[]                                                          ## Tableau rempli de tout les numeros de tests (par la suite)
nbThreads=[1,2,5,10,20,50,100,500]                                      ## Tableau utilisé pour les tests à 1 ou 2 arguments
nbYields=[1,2,5,10,20,50,100,500]                                       ## Tableau utilisé pour les tests à 2 arguments

additionalTestEnable=True                                               ## Autoriser les test additionels

NO_ARG_ITERATIONS=100                                                   ## Nombre de lancements effectués sur les tests à 0 arguments
SMOOTHING=False                                                         ## Activation du lissage des courbes (NON SUPPORTE)
SMOOTHING_ITER=2                                                        ## Nombre de lissages (NON SUPPORTE)
TIME_THRESHOLD=1.0                                                      ## Seuil de temps d'éxecution maximal pour un test
if SMOOTHING:                                                           
    TIME_THRESHOLD=0.5                                                  
REPORT=False                                                            ## Activer le rapport interne de fin

for _ in fichiersTest:                                                  ## Remplissage du tableau des numéros de test
    testNumbers+=[int(_[14:16])]
testNumbers.sort()

## Recherche le test correspondant au testNb. Si pthreads est activé, renvoie le test utilisant pthread.
def searchTest(testNb,pthreads=False,stack=False):
    if pthreads:
        for _ in fichiersTestPthreads:
            if int(_[24:26])==testNb:
                return _
    elif stack:
        for _ in fichiersTestStack:
            if int(_[21:23])==testNb:
                return _
    else:
        for _ in fichiersTest:
            if int(_[14:16])==testNb:
                return _

## Renvoie si le testNb possède une fonction de lancement spécifique
def hasHandler(testNb):
    try:
        foo = globals()["_"+str(testNb)+"Handler"]
        return True
    except:
        return False

## Appelle et renvoie le résultat de la fonction de lancement spécifique du testNb. Si display est activé,
## renvoie le/les tableau/x permettant l'affichage
def testHandler(testNb,pthreads=False,stack=False,display=False):
    return globals()["_"+str(testNb)+"Handler"](pthreads,stack,display)

def _51Handler(pthreads,stack,display):
    newNbThreads=[1,2,5,7,10,12,15,20,22,25,27]
    if display:
        return (newNbThreads,None)
    test=searchTest(51,pthreads,stack)
    results=[]
    for i in newNbThreads:
        start=time.time()
        subprocess.call(test+" "+str(i),shell=True)
        results+=[time.time()-start]
    return results

def noArgHandler(testNb,pthreads,stack,iterations):
    test=searchTest(testNb,pthreads,stack)
    result=0
    for _ in range(iterations):
        start=time.time()
        subprocess.call(test)
        result+=time.time()-start
    return result/iterations

def _01Handler(pthreads,stack,display):
    return noArgHandler(1,pthreads,stack,NO_ARG_ITERATIONS)
def _02Handler(pthreads,stack,display):
    return noArgHandler(2,pthreads,stack,NO_ARG_ITERATIONS)
def _03Handler(pthreads,stack,display):
    return noArgHandler(3,pthreads,stack,NO_ARG_ITERATIONS)
def _11Handler(pthreads,stack,display):
    return noArgHandler(11,pthreads,stack,NO_ARG_ITERATIONS)
def _12Handler(pthreads,stack,display):
    return noArgHandler(12,pthreads,stack,NO_ARG_ITERATIONS)



## Ajuste le tableau resultToFill pour le rendre de même dimension que le tableau resultArray
def resultAdjust(resultArray,resultToFill):
    try:
        if type(resultArray[0])==float:
            n = len(resultArray)
            while len(resultToFill)<n:
                resultToFill+=[resultToFill[-1]]
            while len(resultToFill)>n:
                resultToFill=resultToFill[:-1]
        else: 
            n = len(resultArray)
            m = len(resultArray[0])
            while len(resultToFill[0])<m:
                for i in range(len(resultToFill)):
                    resultToFill[i]+=[resultToFill[i][-1]]
                print("in+")
            while len(resultToFill[0])>m:
                for i in range(len(resultToFill)):
                    resultToFill[i]=resultToFill[i][:-1]
                print(_,"in-")
            while len(resultToFill)<n:
                resultToFill+=[resultToFill[-1] for _ in range(m)]
            while len(resultToFill)>n:
                resultToFill=resultToFill[:-1]
        return resultToFill
    except:
        print("resultAdjust passed")
        return resultToFill

## Lance des tests additionels tant que le TIME_THRESHOLD n'est pas dépassé
def additionalTest(previousResults,test,initialThreads,initialYields,newThread,newYield):
    if newThread >10000 :
        return previousResults
    if newYield!=None and newThread+newYield>10000:
        print(previousResults)
        return previousResults
    if initialThreads==None or initialYields==None: #Un argument
        start=time.time()
        subprocess.call(test+" "+str(newThread),shell=True)
        end=time.time()-start
        if end<TIME_THRESHOLD:
            return additionalTest(previousResults+[end],test,None,None,newThread+1000,None)
        else:
            return previousResults+[end]
    else: # Deux arguments
        for i in initialThreads:
            start = time.time()
            subprocess.call(test+" "+str(i)+" "+str(newYield),shell=True)
            previousResults[initialThreads.index(i)].append(time.time()-start)
        previousResults.append([])
        for j in initialYields+[newYield]:
            start = time.time()
            subprocess.call(test+" "+str(newThread)+" "+str(j),shell=True)
            previousResults[-1].append(time.time()-start)
        if previousResults[-1][-1]<TIME_THRESHOLD:
            return additionalTest(previousResults,test,initialThreads+[newThread],initialYields+[newYield],newThread+1000,newYield+1000)
        else:
            return previousResults
        

## Renvoie les résultats de performance de testNb
def givePerformanceTest(testNb,pthreads,stack):
    test=searchTest(testNb,pthreads,stack)
    if hasHandler(testNb):
            return testHandler(testNb,pthreads,stack)
    if testNb in testWithArguments:
        if nbOfArg[testWithArguments.index(testNb)]==2: # Deux arguments
            results=[[] for _ in range(len(nbThreads))]
            for i in nbThreads: #Threads puis Yields
                for j in nbYields:
                    start=time.time()
                    subprocess.call(test+" "+str(i)+" "+str(j),shell=True)
                    results[nbThreads.index(i)]+=[time.time()-start]
            if additionalTestEnable and results[-1][-1]<TIME_THRESHOLD:
                return additionalTest(results,test,nbThreads,nbYields,nbThreads[-1]-(nbThreads[-1]%1000)+1000,nbYields[-1]-(nbYields[-1]%1000)+1000)
            else:
                return results
        else: # Un argument
            results=[]
            for i in nbThreads:
                start=time.time()
                subprocess.call(test+" "+str(i),shell=True)
                results+=[time.time()-start]
            if additionalTestEnable and results[-1]<TIME_THRESHOLD:
                return additionalTest(results,test,None,None,nbThreads[-1]-(nbThreads[-1]%1000)+1000,None)
            else:
                return results
    else: # Pas d'arguments
        start=time.time()
        print(test)
        subprocess.call(test,shell=True)
        total=time.time()-start
        return total

# def smoother(testNb,pthreads):
#     try:
#         if SMOOTHING:
#             smoothRes=givePerformanceTest(testNb,pthreads)
#             for i in range(SMOOTHING_ITER):
#                 res = givePerformanceTest(testNb,pthreads)
#                 res=resultAdjust(smoothRes,res)
#                 for j in range(len(smoothRes)):
#                     smoothRes[i]+=res[i]
#             return [smoothRes[i]/SMOOTHING_ITER for i in range(len(smoothRes))]
#         else:
#             return givePerformanceTest(testNb,pthreads)
#     except:
#         print("Smoother skipped")

## Affiche l'ensemble des performances
def allPerformances(excludeList=[]):
    singleResults=[]
    singlePthreadResults=[]
    singleStackResults=[]
    fig1 = plt.figure(figsize=(20,10))
    k=1
    fig2 = plt.figure(figsize=(20,10))
    l=1
    for _ in testNumbers:
        if _ not in excludeList:
            time.sleep(1)
            result = givePerformanceTest(_,False,False)
            pthreadsResult = resultAdjust(result,givePerformanceTest(_,True,False))
            stackResult = resultAdjust(result,givePerformanceTest(_,False,True))
            result=np.array(result)
            pthreadsResult=np.array(pthreadsResult)
            stackResult=np.array(stackResult)
            print(result, result.shape)
            shape=result.shape
            print(pthreadsResult,pthreadsResult.shape)
            if shape==(): # Tests sans arguments
                singleResults+=[result]
                singlePthreadResults+=[pthreadsResult]
                singleStackResults+=[stackResult]
            else:
                if hasHandler(_):   
                    realNbThreads,realNbYields=testHandler(_,display=True)
                else:
                    realNbThreads=nbThreads+[nbThreads[-1]-(nbThreads[-1]%1000)+1000+1000*i for i in range(len(result)-len(nbThreads))]
                    realNbYields=nbYields+[nbYields[-1]-(nbYields[-1]%1000)+1000+1000*i for i in range(len(result)-len(nbYields))]
                if len(shape)==1: # Tests à 1 argument
                    ax1 = fig1.add_subplot(2,3,k)
                    ax1.plot(realNbThreads,result, linewidth=2.0,color="blue",label="Temps d'exécution (s)")
                    ax1.plot(realNbThreads,pthreadsResult, linewidth=2.0,color="orange",label="Temps d'exécution avec pthread (s)")
                    ax1.plot(realNbThreads,stackResult, linewidth=2.0,color="green",label="Temps d'exécution avec stack overflow handler (s)")
                    ax1.set_title("Test n°"+str(_))
                    ax1.set_xlabel("Nombre de threads")
                    ax1.set_ylabel("s")
                    ax1.legend(loc="upper right")
                    k+=1
                else: # Tests à 2 arguments
                    scaleYields=[]
                    for y in realNbYields:
                        scaleYields+=[y]*len(realNbYields)
                    ax2 = fig2.add_subplot(2,2,l,projection="3d")
                    ax2.scatter(realNbThreads*len(realNbThreads), scaleYields, result.flatten(), c="blue",label="Temps d'exécution (s)")
                    ax2.scatter(realNbThreads*len(realNbThreads), scaleYields, pthreadsResult.flatten(),c="orange",label="Temps d'exécution avec pthread (s)")
                    ax2.scatter(realNbThreads*len(realNbThreads), scaleYields, stackResult.flatten(),c="green",label="Temps d'exécution avec stack overflow handler (s)")
                    ax2.set_title("Test n°"+str(_))
                    ax2.set_xlabel("Nombre de threads")
                    ax2.set_ylabel("Nombre de yields")
                    ax2.set_zlabel("s")
                    ax2.legend(bbox_to_anchor=(0.25,1))
                    l+=1
    # if singleResults!=[]:
    #     fig, ax = plt.subplots()
    #     x=0.5+np.arange(len(singleResults))
    #     ax.stem(x,singleResults,basefmt=" ", linefmt="b:",label="Temps d'exécution (s)")
    #     ax.stem(x,singlePthreadResults,basefmt=" ", linefmt="-",label="Temps d'exécution (s)")
    #     ax.legend(loc="upper right")
    #     plt.show()
    plt.show()

## Affiche les performances d'un ensemble spécifique de tests
def specificPerformance(testArray):
    allPerformances([ _ for _ in testNumbers if _ not in testArray])

def __main__():
    # allPerformances(testWithArguments+[81])                                                                   ## Tests à 0 arguments
    # allPerformances([_ for _ in testNumbers if _ not in [31,32,33]])                                          ## Tests à 2 arguments
    # allPerformances([_ for _ in testNumbers if _ not in testWithArguments or _ in [31,32,33] ]+[81])          ## Tests a 1 argument

    # specificPerformance([21])

    allPerformances([71,81,41,63,64,65,91,92]) # Tout les tests

    print(searchTest(12,stack=True))

    if REPORT:
        print("END REPORT")
        print("SMOOTHER=",SMOOTHING)
        print("SMOOTHER_ITER=",SMOOTHING_ITER)
        print("TIME_THRESHOLD=",TIME_THRESHOLD)

__main__()
