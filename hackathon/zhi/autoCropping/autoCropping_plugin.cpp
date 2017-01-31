/* autoCropping_plugin.cpp
 * This is a test plugin, you can use it as a demo.
 * 2016-6-15 : by YourName
 */
 
#include "v3d_message.h"
#include <vector>
#include "autoCropping_plugin.h"
#include <QFileInfo>


#include "basic_surf_objs.h"
#include "../AllenNeuron_postprocessing/sort_swc_IVSCC.h"
#include "neuron_sim_scores.h"
#include "../../../released_plugins/v3d_plugins/swc_to_maskimage/filter_dialog.h"

#include <classification.h>
#include "../../../released_plugins/v3d_plugins/istitch/y_imglib.h"

using namespace std;
Q_EXPORT_PLUGIN2(autoCropping, autoCropping);
 
QStringList autoCropping::menulist() const
{
	return QStringList() 
        //<<tr("Crop")
        <<tr("TrainingSetGeneration")
        <<tr("Prediction")
        <<tr("Help");
}

QStringList autoCropping::funclist() const
{
	return QStringList()
        <<tr("TrainingSetGeneration")
        <<tr("TrainingSetGeneration_bkg")
        <<tr("Prediction")
		<<tr("help");
}

NeuronTree cropSWCfile(NeuronTree nt, int xb, int xe, int yb, int ye, int type);
NeuronTree cropSWCfile3D(NeuronTree nt, int xb, int xe, int yb, int ye, int zb, int ze, int type);

template <class T> void cropping3D(V3DPluginCallback2 &callback,
                                   T* data1d,
                                   NeuronTree nt,
                                   QString imgname,
                                   V3DLONG *in_sz,
                                   int Wx,
                                   int Wy,
                                   int Wz,
                                   int type,
                                   int offset);


template <class T> void cropping3D_bkg(V3DPluginCallback2 &callback,
                                   T* data1d,
                                   NeuronTree nt,
                                   QString imgname,
                                   V3DLONG *in_sz,
                                   int Wx,
                                   int Wy,
                                   int Wz);

void autoCropping::domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent)
{
    if (menu_name == tr("Crop"))
	{
        v3dhandle curwin = callback.currentImageWindow();
        if (!curwin)
        {
            QMessageBox::information(0, "", "You don't have any image open in the main window.");
            return;
        }

        Image4DSimple* p4DImage = callback.getImage(curwin);

        if (!p4DImage)
        {
            QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
            return;
        }

        unsigned char* data1d = p4DImage->getRawData();
        QString imgname = callback.getImageName(curwin);

        V3DLONG N = p4DImage->getXDim();
        V3DLONG M = p4DImage->getYDim();
        V3DLONG P = p4DImage->getZDim();
        V3DLONG sc = p4DImage->getCDim();

        V3DLONG in_sz[4];
        in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

        int tmpx,tmpy,tmpz;
        LandmarkList listLandmarks = callback.getLandmark(curwin);
        LocationSimple tmpLocation(0,0,0);
        int marknum = listLandmarks.count();
        if(marknum ==0)
        {
            v3d_msg("No markers in the current image, please double check.");
            return;
        }
        bool ok;
        int winSize = QInputDialog::getInteger(parent, "Please specify the block size","size:",50,0,256,1,&ok);
        if (!ok)
            return;

        QString APP2fileName;
        APP2fileName = QFileDialog::getOpenFileName(0, QObject::tr("Open APP2 SWC File"),
                "",
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(APP2fileName.isEmpty())
            return;

        QString NeutubefileName;
        NeutubefileName = QFileDialog::getOpenFileName(0, QObject::tr("Open Neutube SWC File"),
                APP2fileName,
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(NeutubefileName.isEmpty())
            return;

        QString GSfileName;
        GSfileName = QFileDialog::getOpenFileName(0, QObject::tr("Open Gold Standard SWC File"),
                APP2fileName,
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(GSfileName.isEmpty())
            return;

        NeuronTree nt_app2, nt_neutube, nt_gs;
        nt_app2 = readSWC_file(APP2fileName);
        nt_neutube = readSWC_file(NeutubefileName);
        nt_gs = readSWC_file(GSfileName);

        for (int i=0;i<marknum;i++)
        {
            tmpLocation = listLandmarks.at(i);
            tmpLocation.getCoord(tmpx,tmpy,tmpz);

            V3DLONG xb = tmpx-1-winSize; if(xb<0) xb = 0;
            V3DLONG xe = tmpx-1+winSize; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-winSize; if(yb<0) yb = 0;
            V3DLONG ye = tmpy-1+winSize; if(ye>=M-1) ye = M-1;


            QString outimg_file_app2 = imgname + QString("_x%1_x%2_y%3_y%4_app2.swc").arg(xb).arg(xe).arg(yb).arg(ye);
            QString outimg_file_neutube = imgname + QString("_x%1_x%2_y%3_y%4_neutube.swc").arg(xb).arg(xe).arg(yb).arg(ye);
            QString outimg_file_gs = imgname + QString("_x%1_x%2_y%3_y%4_gs.swc").arg(xb).arg(xe).arg(yb).arg(ye);


            NeuronTree nt_app2_cropped =  cropSWCfile(nt_app2,xb,xe,yb,ye,-1);
            NeuronTree nt_app2_sort;
            if(nt_app2_cropped.listNeuron.size()>0)
            {
                nt_app2_sort = SortSWC_pipeline(nt_app2_cropped.listNeuron,nt_app2_cropped.listNeuron.at(0).n, 0);
            }
            else
                nt_app2_sort = nt_app2_cropped;

            NeuronTree nt_neutube_cropped =  cropSWCfile(nt_neutube,xb,xe,yb,ye,-1);
            NeuronTree nt_neutube_sort;
            if(nt_neutube_cropped.listNeuron.size()>0)
            {
                nt_neutube_sort = SortSWC_pipeline(nt_neutube_cropped.listNeuron,nt_neutube_cropped.listNeuron.at(0).n, 0);
            }else
                nt_neutube_sort = nt_neutube_cropped;

            NeuronTree nt_gs_cropped =  cropSWCfile(nt_gs,xb,xe,yb,ye,-1);
            NeuronTree nt_gs_sort;
            if(nt_gs_cropped.listNeuron.size()>0)
            {
                nt_gs_sort = SortSWC_pipeline(nt_gs_cropped.listNeuron,nt_gs_cropped.listNeuron.at(0).n, 0);
            }else
                nt_gs_sort = nt_gs_cropped;

            writeSWC_file(outimg_file_app2,nt_app2_sort);
            writeSWC_file(outimg_file_neutube,nt_neutube_sort);
            writeSWC_file(outimg_file_gs,nt_gs_sort);


            NeuronDistSimple score_app2 = neuron_score_rounding_nearest_neighbor(&nt_gs_sort, &nt_app2_sort,false);
            NeuronDistSimple score_neutube = neuron_score_rounding_nearest_neighbor(&nt_gs_sort, &nt_neutube_sort,false);


            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = P;
            im_cropped_sz[3] = sc;

            unsigned char *im_cropped = 0;
            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for image_mip."); return;}
            V3DLONG j = 0;
            for(V3DLONG iz = 0; iz < P; iz++)
            {
                V3DLONG offsetk = iz*M*N;
                for(V3DLONG iy = yb; iy <= ye; iy++)
                {
                    V3DLONG offsetj = iy*N;
                    for(V3DLONG ix = xb; ix <= xe; ix++)
                    {
                         im_cropped[j] = data1d[offsetk + offsetj + ix];
                         j++;
                    }
                }
            }
            QString outimg_file;
            if(score_app2.dist_allnodes < score_neutube.dist_allnodes)
                outimg_file = imgname + QString("_x%1_x%2_y%3_y%4_APP2.tif").arg(xb).arg(xe).arg(yb).arg(ye);
            else
                outimg_file = imgname + QString("_x%1_x%2_y%3_y%4_NEUTUBE.tif").arg(xb).arg(xe).arg(yb).arg(ye);

            simple_saveimage_wrapper(callback, outimg_file.toStdString().c_str(),(unsigned char *)im_cropped,im_cropped_sz,1);
            if(im_cropped) {delete []im_cropped; im_cropped = 0;}
        }
        v3d_msg("Done!");
	}
    else if (menu_name == tr("TrainingSetGeneration"))
    {
        v3dhandle curwin = callback.currentImageWindow();
        if (!curwin)
        {
            QMessageBox::information(0, "", "You don't have any image open in the main window.");
            return;
        }

        Image4DSimple* p4DImage = callback.getImage(curwin);

        if (!p4DImage)
        {
            QMessageBox::information(0, "", "The image pointer is invalid. Ensure your data is valid and try again!");
            return;
        }

        unsigned char* data1d = p4DImage->getRawData();
        QString imagename = callback.getImageName(curwin);

        V3DLONG N = p4DImage->getXDim();
        V3DLONG M = p4DImage->getYDim();
        V3DLONG P = p4DImage->getZDim();
        V3DLONG sc = p4DImage->getCDim();

        V3DLONG in_sz[4];
        in_sz[0] = N; in_sz[1] = M; in_sz[2] = P; in_sz[3] = sc;

        bool ok1, ok2, ok3;
        unsigned int Wx=1, Wy=1, Wz=1;

        Wx = QInputDialog::getInteger(parent, "Window X ",
                                      "Enter radius (window size is 2*radius+1):",
                                      50, 1, N, 1, &ok1);

        if(ok1)
        {
            Wy = QInputDialog::getInteger(parent, "Window Y",
                                          "Enter radius (window size is 2*radius+1):",
                                          50, 1, M, 1, &ok2);
        }
        else
            return;

        if(ok2)
        {
            Wz = QInputDialog::getInteger(parent, "Window Z",
                                          "Enter radius (window size is 2*radius+1):",
                                          25, 1, P, 1, &ok3);
        }
        else
            return;


        QString SWCfileName;
        SWCfileName = QFileDialog::getOpenFileName(0, QObject::tr("Open SWC File"),
                "",
                QObject::tr("Supported file (*.swc *.eswc)"
                    ";;Neuron structure	(*.swc)"
                    ";;Extended neuron structure (*.eswc)"
                    ));
        if(SWCfileName.isEmpty())
            return;

        bool ok;
        int type = QInputDialog::getInteger(parent, "Please specify the node type to be cropped (-1 for all types)","type:",-1,-1,256,1,&ok);
        if (!ok)
            return;

        QString outputfolder = imagename + QString("_x%1_y%2_z%3/").arg(Wx).arg(Wy).arg(Wz);
       // system(qPrintable(QString("mkdir %1").arg(outputfolder.toStdString().c_str())));
        QDir().mkdir(outputfolder);

        NeuronTree nt = readSWC_file(SWCfileName);
        ImagePixelType pixeltype = p4DImage->getDatatype();

        switch (pixeltype)
        {
        case V3D_UINT8: cropping3D(callback,data1d, nt, outputfolder, in_sz, Wx, Wy, Wz,type,0); break;
        default:
            v3d_msg("This plugin only supports 8bit datatype for now.");
            if (data1d) {delete []data1d; data1d=0;}
            return;
        }

        v3d_msg(QString("Save all cropped files to %1 folder!").arg(outputfolder));

    }
    else if (menu_name == tr("Prediction"))
    {
        string model_file = "/local1/work/caffe/models/bvlc_reference_caffenet/deploy.prototxt";
        string trained_file = "/local1/work/caffe/models/bvlc_reference_caffenet/caffenet_train_iter_2nd_450000.caffemodel";
        string mean_file = "/local1/work/caffe/data/ilsvrc12/imagenet_mean.binaryproto";
        string label_file = "/local1/work/caffe/data/ilsvrc12/synset_IVSCC.txt";
        Classifier classifier(model_file, trained_file, mean_file, label_file);


        QString m_InputfolderName = 0;
        m_InputfolderName = QFileDialog::getExistingDirectory(parent, QObject::tr("Choose the directory including all images "),
                                              QDir::currentPath(),
                                              QFileDialog::ShowDirsOnly);
        if(m_InputfolderName == 0)
            return;

        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();
        std::vector<cv::Mat> imgs;
        for (V3DLONG i = 0; i <NTILES; i++)
        {
            cv::Mat img = cv::imread(vim.tilesList.at(i).fn_image.c_str());
            imgs.push_back(img);
        }
        std::vector<std::vector<Prediction> > all_predictions = classifier.Classify(imgs);
        V3DLONG p_num = 0;
        V3DLONG n_num = 0;

        /* Print the top N predictions. */
        for (size_t i = 0; i < all_predictions.size(); ++i)
        {
            std::vector<Prediction>& predictions = all_predictions[i];
            Prediction p = predictions[0];
            if(p.first == "0") n_num++;
            if(p.first == "1") p_num++;
        }

        double p_rate = p_num/(p_num+n_num);
        double n_rate = n_num/(p_num+n_num);
        printf("Positive rate is %.2f, and negative rate is %.2f\n",p_rate,n_rate);
        imgs.clear();

    }
    else
	{
		v3d_msg(tr("This is a test plugin, you can use it as a demo.. "
			"Developed by YourName, 2016-6-15"));
	}
}

NeuronTree cropSWCfile(NeuronTree nt, int xb, int xe, int yb, int ye, int type)
{
    //NeutronTree structure
    NeuronTree nt_prunned;
    QList <NeuronSWC> listNeuron;
    QHash <int, int>  hashNeuron;
    listNeuron.clear();
    hashNeuron.clear();

    //set node

    QList<NeuronSWC> list = nt.listNeuron;
    NeuronSWC S;
    for (int i=0;i<list.size();i++)
    {
        NeuronSWC curr = list.at(i);
        if(curr.x <= xe && curr.x >=xb && curr.y <= ye && curr.y >=yb && (type == -1 || curr.type == type))
        {
            S.x 	= curr.x-xb;
            S.y 	= curr.y-yb;
            S.n 	= curr.n;
            S.type = curr.type;
            S.z 	= 1;
            S.r 	= curr.r;
            S.pn 	= curr.pn;
            listNeuron.append(S);
            hashNeuron.insert(S.n, listNeuron.size()-1);
        }
   }
   nt_prunned.n = -1;
   nt_prunned.on = true;
   nt_prunned.listNeuron = listNeuron;
   nt_prunned.hashNeuron = hashNeuron;

   return nt_prunned;
}

NeuronTree cropSWCfile3D(NeuronTree nt, int xb, int xe, int yb, int ye, int zb, int ze,int type)
{
    //NeutronTree structure
    NeuronTree nt_prunned;
    QList <NeuronSWC> listNeuron;
    QHash <int, int>  hashNeuron;
    listNeuron.clear();
    hashNeuron.clear();

    //set node

    QList<NeuronSWC> list = nt.listNeuron;
    NeuronSWC S;
    for (int i=0;i<list.size();i++)
    {
        NeuronSWC curr = list.at(i);
        if(curr.x <= xe && curr.x >=xb && curr.y <= ye && curr.y >=yb && curr.z <= ze && curr.z >=zb && (type == -1 || curr.type == type))
        {
            S.x 	= curr.x-xb;
            S.y 	= curr.y-yb;
            S.z 	= curr.z-zb;
            S.n 	= curr.n;
            S.type = curr.type;
            S.r 	= curr.r;
            S.pn 	= curr.pn;
            listNeuron.append(S);
            hashNeuron.insert(S.n, listNeuron.size()-1);
        }
   }
   nt_prunned.n = -1;
   nt_prunned.on = true;
   nt_prunned.listNeuron = listNeuron;
   nt_prunned.hashNeuron = hashNeuron;

   return nt_prunned;
}

bool autoCropping::dofunc(const QString & func_name, const V3DPluginArgList & input, V3DPluginArgList & output, V3DPluginCallback2 & callback,  QWidget * parent)
{
    vector<char*> infiles, paras, outfiles;
    if(input.size() >= 1) infiles = *((vector<char*> *)input.at(0).p);
    if(input.size() >= 2) paras = *((vector<char*> *)input.at(1).p);
    if(output.size() >= 1) outfiles = *((vector<char*> *)output.at(0).p);

    if (func_name == tr("TrainingSetGeneration"))
	{
        cout<<"Welcome to auto cropping plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image"<<endl;
            return false;
        }
        QString  imagename =  infiles[0];
        int k=0;

        QString SWCfileName = paras.empty() ? "" : paras[k]; if(SWCfileName == "NULL") SWCfileName = ""; k++;
        if(SWCfileName.isEmpty())
        {
            cerr<<"Need a SWC file"<<endl;
            return false;
        }

        int Wx = (paras.size() >= k+1) ? atoi(paras[k]) : 50; k++;
        int Wy = (paras.size() >= k+1) ? atoi(paras[k]) : 50; k++;
        int Wz = (paras.size() >= k+1) ? atoi(paras[k]) : 25;  k++;
        int offset = (paras.size() >= k+1) ? atoi(paras[k]) : 0;  k++;
        int type = (paras.size() >= k+1) ? atoi(paras[k]) : -1;  k++;


        cout<<"inimg_file = "<<imagename.toStdString().c_str()<<endl;
        cout<<"outimg_file = "<<SWCfileName.toStdString().c_str()<<endl;
        cout<<"Wx = "<<Wx<<endl;
        cout<<"Wy = "<<Wy<<endl;
        cout<<"Wz = "<<Wz<<endl;
        cout<<"Flag = "<<offset<<endl;
        cout<<"Type = "<<type<<endl;


        unsigned char * data1d = 0;
        V3DLONG in_sz[4];

        int datatype;
        if(!simple_loadimage_wrapper(callback, imagename.toStdString().c_str(), data1d, in_sz, datatype))
        {
            cerr<<"load image "<<imagename.toStdString().c_str()<<" error!"<<endl;
            return false;
        }

        //flip the image
        if(1)
        {
            V3DLONG hsz1=floor((double)(in_sz[1]-1)/2.0); if (hsz1*2<in_sz[1]-1) hsz1+=1;

            for (k=0;k<in_sz[2];k++)
            {
                V3DLONG offsetk = k*in_sz[1]*in_sz[0];
                for (int j=0;j<hsz1;j++)
                    for (int i=0;i<in_sz[0];i++)
                    {
                        unsigned char tmpv = data1d[offsetk+(in_sz[1]-j-1)*in_sz[0] + i];
                        data1d[offsetk+(in_sz[1]-j-1)*in_sz[0] + i] = data1d[offsetk+j*in_sz[0] + i];
                        data1d[offsetk+j*in_sz[0] + i] = tmpv;
                    }
            }
        }


        QString outputfolder = imagename + QString("_x%1_y%2_z%3/").arg(Wx).arg(Wy).arg(Wz);
        //system(qPrintable(QString("mkdir %1").arg(outputfolder.toStdString().c_str())));
        QDir().mkdir(outputfolder);

        NeuronTree nt = readSWC_file(SWCfileName);

        switch (datatype)
        {
        case 1: cropping3D(callback,data1d, nt, outputfolder, in_sz, Wx, Wy, Wz,type,offset); break;
        default:
            v3d_msg("This plugin only supports 8bit datatype for now.");
            if (data1d) {delete []data1d; data1d=0;}
            return false;
        }

        v3d_msg(QString("Save all cropped files to %1 folder!").arg(outputfolder),0);
        if (data1d) {delete []data1d; data1d=0;}
        return true;
	}
    else if(func_name == tr("TrainingSetGeneration_bkg"))
    {
        cout<<"Welcome to auto background cropping plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image"<<endl;
            return false;
        }
        QString  imagename =  infiles[0];
        int k=0;

        QString SWCfileName = paras.empty() ? "" : paras[k]; if(SWCfileName == "NULL") SWCfileName = ""; k++;
        if(SWCfileName.isEmpty())
        {
            cerr<<"Need a SWC file"<<endl;
            return false;
        }

        int Wx = (paras.size() >= k+1) ? atoi(paras[k]) : 50; k++;
        int Wy = (paras.size() >= k+1) ? atoi(paras[k]) : 50; k++;
        int Wz = (paras.size() >= k+1) ? atoi(paras[k]) : 25;  k++;


        cout<<"inimg_file = "<<imagename.toStdString().c_str()<<endl;
        cout<<"outimg_file = "<<SWCfileName.toStdString().c_str()<<endl;
        cout<<"Wx = "<<Wx<<endl;
        cout<<"Wy = "<<Wy<<endl;
        cout<<"Wz = "<<Wz<<endl;

        unsigned char * data1d = 0;
        V3DLONG in_sz[4];

        int datatype;
        if(!simple_loadimage_wrapper(callback, imagename.toStdString().c_str(), data1d, in_sz, datatype))
        {
            cerr<<"load image "<<imagename.toStdString().c_str()<<" error!"<<endl;
            return false;
        }

        //flip the image
        if(1)
        {
            V3DLONG hsz1=floor((double)(in_sz[1]-1)/2.0); if (hsz1*2<in_sz[1]-1) hsz1+=1;

            for (k=0;k<in_sz[2];k++)
            {
                V3DLONG offsetk = k*in_sz[1]*in_sz[0];
                for (int j=0;j<hsz1;j++)
                    for (int i=0;i<in_sz[0];i++)
                    {
                        unsigned char tmpv = data1d[offsetk+(in_sz[1]-j-1)*in_sz[0] + i];
                        data1d[offsetk+(in_sz[1]-j-1)*in_sz[0] + i] = data1d[offsetk+j*in_sz[0] + i];
                        data1d[offsetk+j*in_sz[0] + i] = tmpv;
                    }
            }
        }


        QString outputfolder = imagename + QString("_x%1_y%2_z%3_bkg/").arg(Wx).arg(Wy).arg(Wz);
        QDir().mkdir(outputfolder);

        NeuronTree nt = readSWC_file(SWCfileName);

        switch (datatype)
        {
        case 1: cropping3D_bkg(callback,data1d, nt, outputfolder, in_sz, Wx, Wy, Wz); break;
        default:
            v3d_msg("This plugin only supports 8bit datatype for now.");
            if (data1d) {delete []data1d; data1d=0;}
            return false;
        }

        v3d_msg(QString("Save all cropped files to %1 folder!").arg(outputfolder),0);
        if (data1d) {delete []data1d; data1d=0;}
        return true;
    }else if(func_name == tr("Prediction"))
    {
        cout<<"Welcome to Caffe prediction plugin"<<endl;
        if(infiles.empty())
        {
            cerr<<"Need input image folder"<<endl;
            return false;
        }
        QString  m_InputfolderName =  infiles[0];
        int k=0;

        QString model_file = paras.empty() ? "" : paras[k]; if(model_file == "NULL") model_file = ""; k++;
        if(model_file.isEmpty())
        {
            cerr<<"Need a model_file"<<endl;
            return false;
        }

        QString trained_file = paras.empty() ? "" : paras[k]; if(trained_file == "NULL") trained_file = ""; k++;
        if(trained_file.isEmpty())
        {
            cerr<<"Need a trained_file"<<endl;
            return false;
        }

        QString mean_file = paras.empty() ? "" : paras[k]; if(mean_file == "NULL") mean_file = ""; k++;
        if(mean_file.isEmpty())
        {
            cerr<<"Need a mean_file"<<endl;
            return false;
        }

        QString label_file = paras.empty() ? "" : paras[k]; if(label_file == "NULL") label_file = ""; k++;
        if(label_file.isEmpty())
        {
            cerr<<"Need a label_file"<<endl;
            return false;
        }

        cout<<"inimg_file = "<<m_InputfolderName.toStdString().c_str()<<endl;
        cout<<"model_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"trained_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"mean_file = "<<model_file.toStdString().c_str()<<endl;
        cout<<"label_file = "<<model_file.toStdString().c_str()<<endl;

        Classifier classifier(model_file.toStdString(), trained_file.toStdString(), mean_file.toStdString(), label_file.toStdString());

        QStringList imgList = importSeriesFileList_addnumbersort(m_InputfolderName);

        Y_VIM<REAL, V3DLONG, indexed_t<V3DLONG, REAL>, LUT<V3DLONG> > vim;

        V3DLONG count=0;
        foreach (QString img_str, imgList)
        {
            V3DLONG offset[3];
            offset[0]=0; offset[1]=0; offset[2]=0;

            indexed_t<V3DLONG, REAL> idx_t(offset);

            idx_t.n = count;
            idx_t.ref_n = 0; // init with default values
            idx_t.fn_image = img_str.toStdString();
            idx_t.score = 0;

            vim.tilesList.push_back(idx_t);
            count++;
        }

        int NTILES  = vim.tilesList.size();
        std::vector<cv::Mat> imgs;
        for (V3DLONG i = 0; i <NTILES; i++)
        {
            cv::Mat img = cv::imread(vim.tilesList.at(i).fn_image.c_str());
            imgs.push_back(img);
        }
        std::vector<std::vector<Prediction> > all_predictions = classifier.Classify(imgs);
        double p_num = 0;
        double n_num = 0;

        /* Print the top N predictions. */
        for (size_t i = 0; i < all_predictions.size(); ++i)
        {
            std::vector<Prediction>& predictions = all_predictions[i];
            Prediction p = predictions[0];
            if(p.first == "0") n_num++;
            if(p.first == "1") p_num++;
        }
        cout<<"\positive rate: "<<p_num/all_predictions.size()<<" and negative rate: "<<n_num/all_predictions.size()<<endl;

        if(!outfiles.empty())
        {
            QString  outputfile =  outfiles[0];
            QFile saveTextFile;
            saveTextFile.setFileName(outputfile);
            if (!saveTextFile.isOpen()){
                if (!saveTextFile.open(QIODevice::Text|QIODevice::Append  )){
                    qDebug()<<"unable to save file!";
                    return false;}     }
            QTextStream outputStream;
            outputStream.setDevice(&saveTextFile);
            outputStream<< m_InputfolderName<<"\t"<< NTILES<<"\t"<<p_num<<"\t"<< n_num<<"\n";
            saveTextFile.close();
        }
        imgs.clear();
        return true;
    }
	else if (func_name == tr("help"))
	{
        {
            cout<<"Usage : v3d -x autoCropping -f TrainingSetGeneration -i <inimg_file> -p <inswc_file> <wx> <wy> <wz> <flag> <type>"<<endl;
            cout<<endl;
            cout<<"inimg_file  please specify the path of the input 3D image file, which should be 8 bit image"<<endl;
            cout<<"inswc_file  please specify the path of the input swc file"<<endl;
            cout<<"wx          filter window radius size (pixel #) in x direction, window size is 2*wx+1, default 50"<<endl;
            cout<<"wy          filter window radius size (pixel #) in y direction, window size is 2*wy+1, default 50"<<endl;
            cout<<"wz          filter window radius size (pixel #) in z direction, window size is 2*wz+1, default 25"<<endl;
            cout<<"flag        export the swc file with original corrdiates(1 for yes and 0 for no. Default 0.)"<<endl;
            cout<<"type        the node type to be cropped, please do NOT specify if you want to crop all nodes"<<endl;

            cout<<endl;
            cout<<endl;
            return true;
        }
	}
	else return false;

	return true;
}

template <class T> void cropping3D(V3DPluginCallback2 &callback,
                                   T* data1d,
                                   NeuronTree nt,
                                   QString outputfolder,
                                   V3DLONG *in_sz,
                                   int Wx,
                                   int Wy,
                                   int Wz,
                                   int type,
                                   int offset)
{
    V3DLONG N = in_sz[0];
    V3DLONG M = in_sz[1];
    V3DLONG P = in_sz[2];
    V3DLONG sc = in_sz[3];

    for (int i=0;i<nt.listNeuron.size();i++)
    {
        if(type == -1 || nt.listNeuron.at(i).type == type)
        {
            V3DLONG tmpx = nt.listNeuron.at(i).x;
            V3DLONG tmpy = nt.listNeuron.at(i).y;
            V3DLONG tmpz = nt.listNeuron.at(i).z;

            V3DLONG xb = tmpx-1-Wx; if(xb<0) xb = 0;if(xb>=N-1) xb = N-1;
            V3DLONG xe = tmpx-1+Wx; if(xe>=N-1) xe = N-1;
            V3DLONG yb = tmpy-1-Wy; if(yb<0) yb = 0;if(yb>=M-1) yb = M-1;
            V3DLONG ye = tmpy-1+Wy; if(ye>=M-1) ye = M-1;
            V3DLONG zb = tmpz-1-Wz; if(zb<0) zb = 0;if(zb>=P-1) zb = P-1;
            V3DLONG ze = tmpz-1+Wz; if(ze>=P-1) ze = P-1;

            QString outimg_file = outputfolder + QString("x%1_y%2_z%3.tif").arg(tmpx).arg(tmpy).arg(tmpz);
            QString outimg_file_swc = outputfolder + QString("x%1_y%2_z%3.swc").arg(tmpx).arg(tmpy).arg(tmpz);
            QString outimg_file_linker = outputfolder + QString("x%1_y%2_z%3.ano").arg(tmpx).arg(tmpy).arg(tmpz);
            int p = 1;

            while(QFile(outimg_file).exists())
            {
                outimg_file = outimg_file + QString("_%1.tif").arg(p);
                outimg_file_swc = outimg_file_swc + QString("_%1.swc").arg(p);
                outimg_file_linker = outimg_file_linker + QString("_%1.ano").arg(p);
                p++;
            }


            NeuronTree nt_cropped;
            if(P>1)
                nt_cropped=cropSWCfile3D(nt,xb,xe,yb,ye,zb,ze,type);
            else
                nt_cropped=cropSWCfile(nt,xb,xe,yb,ye,type);

            NeuronTree nt_sort;
            if(nt_cropped.listNeuron.size()>0)
            {
                nt_sort = SortSWC_pipeline(nt_cropped.listNeuron,nt_cropped.listNeuron.at(0).n, 0);
            }
            else
                nt_sort = nt_cropped;

            writeSWC_file(outimg_file_swc,nt_sort);

            if(offset)
            {
                QString outimg_file_swc_offset = outimg_file_swc + "_offset.swc";
                for(V3DLONG ii = 0; ii < nt_sort.listNeuron.size(); ii++)
                {
                    nt_sort.listNeuron[ii].x += xb;
                    nt_sort.listNeuron[ii].y += yb;
                    nt_sort.listNeuron[ii].z += zb;
                }
                writeSWC_file(outimg_file_swc_offset,nt_sort);
            }


            V3DLONG im_cropped_sz[4];
            im_cropped_sz[0] = xe - xb + 1;
            im_cropped_sz[1] = ye - yb + 1;
            im_cropped_sz[2] = ze - zb + 1;
            im_cropped_sz[3] = sc;


            unsigned char *im_cropped = 0;
            V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
            try {im_cropped = new unsigned char [pagesz];}
            catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return;}
            V3DLONG j = 0;
            for(V3DLONG iz = zb; iz <= ze; iz++)
            {
                V3DLONG offsetk = iz*M*N;
                for(V3DLONG iy = yb; iy <= ye; iy++)
                {
                    V3DLONG offsetj = iy*N;
                    for(V3DLONG ix = xb; ix <= xe; ix++)
                    {
                         im_cropped[j] = data1d[offsetk + offsetj + ix];
                         j++;
                    }
                }
            }
            simple_saveimage_wrapper(callback, outimg_file.toStdString().c_str(),(unsigned char *)im_cropped,im_cropped_sz,1);
            QFile qf_anofile(outimg_file_linker);
            if(!qf_anofile.open(QIODevice::WriteOnly))
            {
                v3d_msg("Cannot open file for writing!",0);
                return;
            }

            QTextStream out(&qf_anofile);
            out << "RAWIMG= " << outimg_file.toStdString().c_str()<< endl;
            out << "SWCFILE= " << outimg_file_swc.toStdString().c_str()<< endl;

            if(im_cropped) {delete []im_cropped; im_cropped = 0;}
        }
    }
    return;

}

template <class T> void cropping3D_bkg(V3DPluginCallback2 &callback,
                                   T* data1d,
                                   NeuronTree nt,
                                   QString outputfolder,
                                   V3DLONG *in_sz,
                                   int Wx,
                                   int Wy,
                                   int Wz)
{
    V3DLONG N = in_sz[0];
    V3DLONG M = in_sz[1];
    V3DLONG P = in_sz[2];
    V3DLONG sc = in_sz[3];

    for(V3DLONG ii = 0; ii < nt.listNeuron.size(); ii++)
    {
      //  nt.listNeuron[ii].z = 1;
        nt.listNeuron[ii].r = nt.listNeuron[ii].r*1.5;

    }

    double x_min,x_max,y_min,y_max,z_min,z_max;
    x_min=x_max=y_min=y_max=z_min=z_max=0;
    V3DLONG sx,sy,sz;

    BoundNeuronCoordinates(nt,x_min,x_max,y_min,y_max,z_min,z_max);
    sx=x_max;
    sy=y_max;
    sz=1;
    unsigned char* data1d_mask = 0;
    V3DLONG stacksz = sx*sy*sz*sc;

    data1d_mask = new unsigned char [stacksz];
    memset(data1d_mask,0,stacksz*sizeof(unsigned char));
    ComputemaskImage(nt, data1d_mask, sx, sy, sz);

    unsigned char *pData = new unsigned char[N*M*P];
    memset(pData,0,N*M*P*sizeof(unsigned char));

    for (V3DLONG k1 = 0; k1 < sz; k1++){
        for(V3DLONG j1 = 0; j1 < sy; j1++){
            for(V3DLONG i1 = 0; i1 < sx; i1++){
                pData[k1 * N*M + j1*N + i1] = data1d_mask[k1*sx*sy + j1*sx +i1];
            }
        }
    }

    for (int i=0;i<nt.listNeuron.size();i++)
    {

        V3DLONG tmpx = rand()%(N-1);
        V3DLONG tmpy = rand()%(M-1);
        V3DLONG tmpz = 1;

        V3DLONG xb = tmpx-1-Wx;
        V3DLONG xe = tmpx-1+Wx;
        V3DLONG yb = tmpy-1-Wy;
        V3DLONG ye = tmpy-1+Wy;

        while(pData[tmpy*N+tmpx]!=0 || xb<0 || xe>=N-1 || yb<0 || ye>=M-1)
        {
            tmpx = rand()%(N-1);
            tmpy = rand()%(M-1);
            xb = tmpx-1-Wx;
            xe = tmpx-1+Wx;
            yb = tmpy-1-Wy;
            ye = tmpy-1+Wy;
        }

        V3DLONG zb = 0;
        V3DLONG ze = 0;

        QString outimg_file = outputfolder + QString("x%1_y%2_z%3_bkg.tif").arg(tmpx).arg(tmpy).arg(tmpz);

        V3DLONG im_cropped_sz[4];
        im_cropped_sz[0] = xe - xb + 1;
        im_cropped_sz[1] = ye - yb + 1;
        im_cropped_sz[2] = ze - zb + 1;
        im_cropped_sz[3] = sc;
        unsigned char *im_cropped = 0;
        V3DLONG pagesz = im_cropped_sz[0]* im_cropped_sz[1]* im_cropped_sz[2]*im_cropped_sz[3];
        try {im_cropped = new unsigned char [pagesz];}
        catch(...)  {v3d_msg("cannot allocate memory for im_cropped."); return;}
        V3DLONG j = 0;
        for(V3DLONG iz = zb; iz <= ze; iz++)
        {
            V3DLONG offsetk = iz*M*N;
            for(V3DLONG iy = yb; iy <= ye; iy++)
            {
                V3DLONG offsetj = iy*N;
                for(V3DLONG ix = xb; ix <= xe; ix++)
                {
                    im_cropped[j] = data1d[offsetk + offsetj + ix];
                    V3DLONG test_num = offsetk + offsetj + ix;
                    j++;
                }
            }
        }
        simple_saveimage_wrapper(callback, outimg_file.toStdString().c_str(),(unsigned char *)im_cropped,im_cropped_sz,1);
        if(im_cropped) {delete []im_cropped; im_cropped = 0;}
    }

    if(data1d_mask) {delete []data1d_mask; data1d_mask = 0;}
    if(pData) {delete []pData; pData = 0;}

    return;

}
