#include "cero_modelonepaire.h"

cERO_ModelOnePaire::cERO_ModelOnePaire(int argc, char** argv):mO1(0),mO2(0)
{
    mNbObs=100; // nbTuile: NbObs/2 car 2 observation pour chacune des tuiles, 1ier et 3ieme quartiles des valeurs radiométriques de l'images 1 et pour Im2 la valeur prédicte par le modèle ajusté randsac
    mNameOr1="Ortho1.tif";
    mNameOr1="Ortho2.tif";
    mNameOr1AndDir="./Ortho1.tif";
    mNameOr2AndDir="./Ortho2.tif";
    mDebug=true;
    mPoid1=0;
    mPoid2=1;
    mTmpDirEROS="";
    mNbCplRadio=0;
    mDir="./";

    ElInitArgMain
    (
        argc,argv,
        LArgMain()  << EAMC(mNameOr1AndDir,"Name Ortho 1",eSAM_IsExistFile)
                    << EAMC(mNameOr2AndDir,"Name Ortho 2",eSAM_IsExistFile),
        LArgMain()  << EAM(mNbObs,"NbObs",true,"Number of radiometric couple observation we want to fit the radiometric equalization model, will determine the number of image tiles.")
                    << EAM(mPoid1,"W1",true,"Weight we give to Ortho 1. Default 0, means we want to correct Ortho1 in order to achieve Ortho2 radiometry.")
                    << EAM(mPoid2,"W2",true,"Weight we give to Ortho 2. Default 1, means we want to correct Ortho1 in order to have Ortho2 radiometry. If W1=W2, the model will try to egalize Ortho1 radiometry in order to match the mean radiometry of Ortho1 and Ortho2.")
                    << EAM(mDebug,"Debug",true,"If true, write dozens of useless intermediates results, both in the terminal and in your disk.")
                    << EAM(mTmpDirEROS,"Dir",true,"Directory where to save the results")

                );
    std::cout << "launch Radiometric Egalization of 2 Orthos \n";
    // Initialize name manipulator & files
    SplitDirAndFile(mDir,mNameOr1,mNameOr1AndDir);
    SplitDirAndFile(mDir,mNameOr2,mNameOr2AndDir);
    // define the "working directory" of this session
    mICNM = cInterfChantierNameManipulateur::BasicAlloc(mDir);

    // create directory for result

    mTmpDirERO=mTmpDirEROS+ "ERO-" +mNameOr1+"-"+mNameOr2+"/";
    // purge previous results
    if(ELISE_fp::IsDirectory(mTmpDirERO))
    {
       std::cout << "Purge of directory " << mTmpDirERO << "\n";
        ELISE_fp::PurgeDirGen(mTmpDirERO,1);
    }

    if ( !ELISE_fp::IsDirectory(mTmpDirEROS)) ELISE_fp::MkDirRec(mTmpDirEROS);
    if (mDebug ) ELISE_fp::MkDirRec(mTmpDirERO);

    // contrôle les valeurs de pondérations
    if ((mPoid1+mPoid2)==0)
    {
     mPoid1=0;
     mPoid2=1;
    }

    // Initialise les 2 orthos
    mO1 = new cImGeo(mDir+mNameOr1);
    mO2 = new cImGeo(mDir+mNameOr2);
    // Determine la zone de recouvrement entre les 2 orthos
    mBoxOverlapTerrain=mO1->overlapBox(mO2);
    // clip les 2 ortho sur cette box terrain
    mO1clip = mO1->clipImTer(mBoxOverlapTerrain);
    mO2clip = mO2->clipImTer(mBoxOverlapTerrain);
    /* les 2 Im2D peuvent avoir un pixels de décalage, en y dans mon exemple, pk? pas bon ça
    std::cout << "mO1clip size " << mO1clip.sz() << "\n";
    std::cout << "mO2clip size " << mO2clip.sz() << "\n"; */
    // on veut la meme taille exactement
    Im2D_REAL4 tmp(mO1clip.sz().x,mO1clip.sz().y);
    ELISE_COPY(
                mO2clip.all_pts(),
                mO2clip.in(),
                tmp.oclip()
                );
    mO2clip = tmp;

    if (mDebug){
    saveIm2D(& mO1clip, mTmpDirERO+"Im1clipped.tif");
    saveIm2D(& mO2clip, mTmpDirERO+"Im2clipped.tif");
    }

    // on utilise ransac pour, sur chacune des tuiles, garder 2 couples radiométriques exempt de bruit et représentatif de la tuile
    ransacOnEachTiles();

    // maintenant qu'on a fait le tour de chaque tuile:dalle, on calcul les modele Global

    ransacOn2Orthos();

    // appliquer les modèle sur les 2 orthos isolées et effectue le mosaiquage afin de pouvoir inspecter facilement le résultat
    if (mDebug) applyRE();

    //export the two models in a txt file
    saveModels();

// fin de l'appli
}

void cERO_ModelOnePaire::saveModels()
{
    if (mDebug) {
    // sauver les 2 modèles dans le meme fichier
    std::string file1(mTmpDirERO+"modelsERO.txt");
    FILE * aFOut = FopenNN(file1.c_str(),"w","out");
    fprintf(aFOut,"%f %f\n",mModelER1.getA(),mModelER1.getB());
    fprintf(aFOut,"%f %f\n",mModelER2.getA(),mModelER2.getB());
    ElFclose(aFOut);
    }

    // if mTmpDirEROS have been initilized, it means that EROS call multiple model computation, thus results are written in the same txt file
    // moreover, the model is not save but instead, two couple of radiometric values
    if(EAMIsInit(&mTmpDirEROS)){

    // plus tard, j'aurai peut-être besoin d'une pondération différente pour les 2 images, genre la moyenne de l'incidence
    int aPond1=mNbCplRadio;
    int aPond2=mNbCplRadio;
    Pt2dr aPt11=predQuantile(&mObsGlob,mModelER1,1);
    Pt2dr aPt12=predQuantile(&mObsGlob,mModelER1,3);
    Pt2dr aPt21=predQuantile(&mObsGlob,mModelER2,1,true);
    Pt2dr aPt22=predQuantile(&mObsGlob,mModelER2,3,true);


    std::string file1(mTmpDirEROS+mNameOr1.substr(0, mNameOr1.size()-3) + "txt");
    std::string file2(mTmpDirEROS+mNameOr2.substr(0, mNameOr2.size()-3) + "txt");
    FILE * aFOut1 = FopenNN(file1.c_str(),"app","out");
    FILE * aFOut2 = FopenNN(file2.c_str(),"app","out");

    fprintf(aFOut1,"%f %f %i %s %i %i\n",aPt11.x,aPt11.y,aPond1,mNameOr2.c_str(),mPoid1,mPoid2);
    fprintf(aFOut1,"%f %f %i %s %i %i\n",aPt12.x,aPt12.y,aPond1,mNameOr2.c_str(),mPoid1,mPoid2);
    fprintf(aFOut2,"%f %f %i %s %i %i\n",aPt21.x,aPt21.y,aPond2,mNameOr1.c_str(),mPoid2,mPoid1);
    fprintf(aFOut2,"%f %f %i %s %i %i\n",aPt22.x,aPt22.y,aPond2,mNameOr1.c_str(),mPoid2,mPoid1);

    ElFclose(aFOut1);
    ElFclose(aFOut2);
    }

}


Pt2di cERO_ModelOnePaire::gridSize()
{
   /* int w=mO1clip.sz().x/sqrt(mNbObs);
    int h=mO1clip.sz().y/sqrt(mNbObs);
    return Pt2di(w,h);*/
    // nombre total de couple radiométrique
    int nbPix(0);

    ELISE_COPY(
                select(mO1clip.all_pts(),mO1clip.in()!=0  && mO2clip.in()!=0),
                1,
                sigma(nbPix)
                );

    int largeurTuile(std::sqrt(nbPix/mNbObs));
    return Pt2di(largeurTuile,largeurTuile);
}

cERO_ModelOnePaire::~cERO_ModelOnePaire()
{
    delete mO1;
    delete mO2;
}


void cERO_ModelOnePaire::saveIm2D(Im2D<float, double> * aIm,std::string aName)
{
    ELISE_COPY(
                aIm->all_pts(),
                aIm->in(),
                Tiff_Im(aName.c_str(),
                            aIm->sz(),
                            GenIm::real4,
                            Tiff_Im::No_Compr,
                            Tiff_Im::BlackIsZero).out()
                );
}

void cERO_ModelOnePaire::applyRE() // pour le moment; applique la correction radiométrique et écrit le résultat dans un nouveau fichier images Tiff
{

    // on travaille sur l'image numéro 1
    std::string filename(mTmpDirERO+"Im1_egalized.tif");
    Im2D_REAL4 im1(mO1->applyRE(mModelER1));
    // je sauve le résultats
    saveIm2D(&im1,filename);

    // image 2
    filename=mTmpDirERO+"Im2_egalized.tif";
    Im2D_REAL4 im2(mO2->applyRE(mModelER2));
    // je sauve le résultats
    saveIm2D(&im2,filename);

    // mosaique des 2 images egalizées
    Box2dr boxMosaic=mO1->boxEnglob(mO2);
    // generate im for the result. should ideally also generate a tfw from the boxMosaic and the GSD

    Im2D_REAL4 mosaic=mO1->box2Im(boxMosaic);
    Pt2dr aCorner=Pt2dr(boxMosaic._p0.x,boxMosaic._p1.y); // xmin, ymax;
    Pt2di tr1=mO1->computeTrans(aCorner), tr2=mO2->computeTrans(aCorner);
    mO1->loadIncid();
    mO2->loadIncid();
    //saveIm2D(mO1->Incid(),mTmpDirERO+"Incid2.tif");

    // copy o1 in the mosaic
    ELISE_COPY(select(mosaic.all_pts(),trans(im1.in(0),tr1)!=0),
               trans(im1.in(0),tr1), // si pas in(0), foire le bazard
               mosaic.out()
                );

    // copy o2 in the mosaic only for pixel that have lower incidence value than o1
    ELISE_COPY(select(mosaic.all_pts(),trans(im2.in(0),tr2)!=0 && trans(mO2->Incid()->in(10),tr2)<trans(mO1->Incid()->in(10),tr1)),
               trans(im2.in(0),tr2),
               mosaic.out()
                );

    saveIm2D(&mosaic,mTmpDirERO+"mosaicRE.tif");

    // pour comparaison, la mosaique sans correction radiométrique
    // j'écrase les object im1, im2 et mosaic
    im1=mO1->toRAM();
    im2=mO2->toRAM();
    ELISE_COPY(mosaic.all_pts(),0,mosaic.out());

    // copy o1 in the mosaic
    ELISE_COPY(select(mosaic.all_pts(),trans(im1.in(0),tr1)!=0),
               trans(im1.in(0),tr1), // si pas in(0), foire le bazard
               mosaic.out()
                );

    // copy o2 in the mosaic only for pixel that have lower incidence value than o1
    ELISE_COPY(select(mosaic.all_pts(),trans(im2.in(0),tr2)!=0 && trans(mO2->Incid()->in(10),tr2)<trans(mO1->Incid()->in(10),tr1)),
               trans(im2.in(0),tr2),
               mosaic.out()
                );

    saveIm2D(&mosaic,mTmpDirERO+"mosaic.tif");

}



double cERO_ModelOnePaire::pond(Pt2dr aPt)
{
    return (mPoid1*aPt.x+mPoid2*aPt.y)/(mPoid1+mPoid2);
}

void cERO_ModelOnePaire::ransacOnEachTiles()
{
    // Détermine la taille de la tuille en fonction du nombre d'observation qu'on souhaite et du nombre de couple radiométrique total
    mSzTuile=gridSize();

    if (mDebug) std::cout << "Taille Tuile" << mSzTuile << "\n";
    int loopX=mO1clip.sz().x/mSzTuile.x;
    int loopY=mO1clip.sz().y/mSzTuile.y;

    for (int i(0) ; i<loopX;i++)
    {
        for (int j(0) ; j<loopY;j++)
        {
            // travail sur la tuile courante
            Pt2di BoxPix(i*mSzTuile.x,j*mSzTuile.y);
            Im2D_REAL4 dO1(mSzTuile.x,mSzTuile.y); //dO1: dalle courrante Ortho 1
            Im2D_REAL4 dO2(mSzTuile.x,mSzTuile.y);

            ELISE_COPY(
                        mO1clip.all_pts(),
                        trans(mO1clip.in(0),BoxPix),
                        dO1.oclip()
                        );

            ELISE_COPY(
                        mO2clip.all_pts(),
                        trans(mO2clip.in(0),BoxPix),
                        dO2.oclip()
                        );

            // test pour voir si il y bien des données dans les 2 tuiles
            int nbPix(0);
            // liste de points, utilisée pour mémoriser le flux de points ou il y a des données radiométrique dans les 2 orthos.
            Liste_Pts_INT2 fluxPt(2); // je sais pas si j'en aurai besoin
            ELISE_COPY(
                        select(dO1.all_pts(),dO1.in()!=0 && dO2.in()!=0),
                        1,
                        sigma(nbPix) | fluxPt
                        );
            // au passage, calcul du nombre total de couples radiometrique
            mNbCplRadio+=nbPix;
            // au minimum 25% de la tuile avec des couples radiométriques
            if (nbPix>(mSzTuile.x*mSzTuile.y)/4)
            {

            if(mDebug){
           // std::cout << "Tuile courante : i " << i << " , j " << j << " nb common pixels: " << nbPix << ". \n";
            /*std::string filename1=mTmpDirERO+"Tuile_"+std::to_string(i)+ "_" + std::to_string(j)+"_O1.tif";
            std::string filename2=mTmpDirERO+"Tuile_"+std::to_string(i)+ "_" + std::to_string(j)+"_O2.tif";
            saveIm2D(& dO1, filename1);
            saveIm2D(& dO2, filename2);
            */
                }
            // sur la dalle : effectue un filtre median de taille de fenetre de 5x5, le filtre médian va donc filtrer les outliers déjà à ce premier niveaux

            ELISE_COPY(
                        dO1.all_pts(),
                        rect_median(dO1.in_proj(),5,65535),// pk une valeur max pour cette fonction?? je sais pas.
                        dO1.out()
                        );
            ELISE_COPY(
                        dO2.all_pts(),
                        rect_median(dO2.in_proj(),5,65535),
                        dO2.out()
                        );

            // boucle sur chaque pixel de la tuile
            for (int v(0); v < mSzTuile.y; v++)
            {
                for (int u(0); u < mSzTuile.x; u++)
                {

                    Pt2di aPos(u,v);
                    double aVal1 = dO1.GetR(aPos);
                    double aVal2 = dO2.GetR(aPos);

                    // couple radiométrique
                    if (aVal1!=0 && aVal2!=0)
                    {

                        // ajout du couple dans le vecteur d'observation
                        mObsDallage.push_back(Pt2dr(aVal1,aVal2));
                    }
                }
            }

            // j'utilise la liste de ces couples d'observations radiométriques pour déterminer un modèle linéaire par RANSAC

            cRansac_2dline aRasac(&mObsDallage,a_plus_bx,50);

            //if(mDebug) std::cout << "lancement de ransac sur la tuile\n";
            aRasac.adjustModel();

            // pour chacune des dalles, je récupère 2 couples radiométriques prédit avec le modèle linéaire sur les observation du 1 et 3ieme quartile
            // ce sont ces 2 observation qui sont utilisées pour le modèle d'égalisation pour la paire d'image
            mObsGlob.push_back(predQuantile(&mObsDallage,aRasac.getModel(),1));
            mObsGlob.push_back(predQuantile(&mObsDallage,aRasac.getModel(),3));

            //if(mDebug) std::cout << "fin de ransac  sur la tuile\n";
            // fin travail sur la dalle
            mObsDallage.clear();

            //std::cout << "taille de mObsDallage " << mObsDallage.size() << " \n";
            }
        // passe à la dalle suivante
        }
    // passe à la dalle suivante
    }
}

Pt2dr cERO_ModelOnePaire::predQuantile(std::vector<Pt2dr> * aObs, c2DLineModel aMod, int aQuartile,bool Y)
{
    // if bool Y=true, mean we are interested in y value of the pt2d, problem is that sort method work only on first element of Pt2d. poor and dangerous Solution is to flip the observations
    int quartile=aObs->size()/4;

    if (!Y)
    {

    std::sort (aObs->begin(), aObs->end());

    // aQuartile==1 ou 3
    return Pt2dr(aObs->at(quartile*aQuartile).x,aMod.predict(aObs->at(quartile*aQuartile).x));
    }
    else
    {// inversion du vector Pt2d et copie dans un autre temporaire
        std::vector<Pt2dr> aObsYX;
        for(auto & pt: *aObs)
        {
        aObsYX.push_back(pt.yx());
        }
        std::sort (aObsYX.begin(), aObsYX.end());
        // aQuartile==1 ou 3
        return Pt2dr(aObsYX.at(quartile*aQuartile).x,aMod.predict(aObsYX.at(quartile*aQuartile).x));
     }
}

void cERO_ModelOnePaire::ransacOn2Orthos()
{
    // on utilise les valeurs de pondération W1 et W2 pour savoir à quoi sert le modèle calculé

    // W1=0 et W2=1: on veut corriger im1 mais pas im2.
    // W1=1 et W2=0: renvoie un modèle "unité", a=0 et b=1--> on veut corriger Im2 mais pas Im1
    // W1=W2 : on veut corriger et Im1 et Im2 afin d'obtenir une radiométrie moyenne à ces deux images.

    //plusieurs situation . 1iere, un des deux poids est 0


    if (mPoid1==0 || mPoid2==0)
    {
        // modèle im 1 vers im 2
        if (mPoid1==0){

            if (mDebug) std::cout << "Radiom Im1 toward radiom Im2 \n";
            cRansac_2dline aRansac(&mObsGlob,a_plus_bx,100);
            aRansac.adjustModel();
            if (mDebug) aRansac.affiche();
            mModelER1=aRansac.getModel();
            // modèle "unité"
            mModelER2=c2DLineModel();
        }

        // modèle im 2 vers im 1
        if (mPoid2==0){
             if (mDebug) std::cout << "Radiom Im2 toward radiom Im1 \n";
            // modifie le vecteur d'information: inverse le couple radiométrique
            std::vector<Pt2dr> aObs;
            for (unsigned int i(0); i<mObsGlob.size();i++) aObs.push_back(Pt2dr(mObsGlob.at(i).yx()));
            cRansac_2dline aRansac(&aObs,a_plus_bx,100);
            aRansac.adjustModel();
            if (mDebug) aRansac.affiche();
            mModelER2=aRansac.getModel();
            mModelER1=c2DLineModel();
        }
    }
    if (mPoid1!=0 && mPoid2!=0)
    {
         if (mDebug) std::cout << "Correct Im1 and Im2 with weighting " << mPoid1 << " and " << mPoid2 << " respectively \n";
        // création des vecteurs d'observation pour les 2 images
        std::vector<Pt2dr> aObs1,aObs2;
        for (unsigned int i(0); i<mObsGlob.size();i++) aObs1.push_back(Pt2dr(mObsGlob.at(i).x,pond(mObsGlob.at(i))));
        for (unsigned int i(0); i<mObsGlob.size();i++) aObs2.push_back(Pt2dr(mObsGlob.at(i).y,pond(mObsGlob.at(i))));

        // ajustement des modèles
        cRansac_2dline aRansac(&aObs1,a_plus_bx,100);
        aRansac.adjustModel();
        if (mDebug) aRansac.affiche();
        mModelER1=aRansac.getModel();

        cRansac_2dline aRansac2(&aObs2,a_plus_bx,100);
        aRansac2.adjustModel();
        if (mDebug) aRansac2.affiche();
        mModelER2=aRansac2.getModel();
    }
}

int main_ero(int argc,char ** argv)
{
    cERO_ModelOnePaire(argc,argv);
   return EXIT_SUCCESS;
}
