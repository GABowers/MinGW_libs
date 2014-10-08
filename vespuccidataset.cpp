/************************************************************************************
    Copyright (C) 2014 Daniel P. Foose - All Rights Reserved

    This file is part of Vespucci.

    Vespucci is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Vespucci is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Vespucci.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************************/

#include "vespuccidataset.h" //VespucciDataset includes all necessary headers.



using namespace arma;
using namespace std;

VespucciDataset::VespucciDataset()
{

}

///
/// \brief VespucciDataset::~VespucciDataset
/// Destructor deletes everything allocated with new that isn't a smart pointer
VespucciDataset::~VespucciDataset()
{
    //make sure principal components stats are deleted properly.
    if (principal_components_calculated_)
        delete principal_components_data_;
    if (partial_least_squares_calculated_)
        delete partial_least_squares_data_;

    //make sure all maps are delted properly.
    for (int i = 0; i < maps_.size(); ++i)
        RemoveMapAt(i);

    DestroyLogFile();

}

bool VespucciDataset::Save(QString filename)
{
    field<mat> dataset(4);
    dataset(0) = spectra_;
    dataset(1) = x_;
    dataset(2) = y_;
    dataset(3) = wavelength_;
    bool success = dataset.save(filename.toStdString(), arma_binary);
    return success;

}

///
/// \brief VespucciDataset::DestroyLogFile
/// Deletes log file unless user decides to save it elsewhere
void VespucciDataset::DestroyLogFile()
{

    QMessageBox::StandardButton reply =
            QMessageBox::question(main_window_,
                                  "Save log?",
                                  "Would you like to save the log for " + name_
                                  + "?",
                                  QMessageBox::Yes|QMessageBox::No);
    QString filename;


    if (reply == QMessageBox::No){
        log_file_->remove();
        return;
    }
    else{
        filename = QFileDialog::getSaveFileName(0, QTranslator::tr("Save File"),
                                   *directory_,
                                   QTranslator::tr("Text Files (*.txt)"));

        bool success = log_file_->copy(filename);
        if (!success){
            bool remove_success = QFile::remove(filename); //delete old file
            if (!remove_success){
                QMessageBox::warning(main_window_, "Failure", "Previous file could not be removed");
            }
            success = log_file_->copy(filename);
        }
        //new log file falls out of scope
        log_file_->remove();

        if (success)
            QMessageBox::information(main_window_, "Success!", "File " + filename + " written successfully");
        else
            QMessageBox::warning(main_window_, "Failure", "File not written successfully.");
    }
    return;
}

///
/// \brief VespucciDataset::VespucciDataset
/// \param binary_filename The filename of the binary input file
/// \param main_window The main window of the program
/// \param directory The current working directory
/// \param name The name of the dataset displayed to the user
/// \param log_file The log file
/// Constructor for loading saved spectral/spatial data in armadillo format
VespucciDataset::VespucciDataset(QString binary_filename,
                                 MainWindow *main_window,
                                 QString *directory,
                                 QString name,
                                 QFile *log_file): log_stream_(log_file)
{
    QDateTime datetime = QDateTime::currentDateTimeUtc();
    log_file_ = log_file;
    log_stream_ << "Vespucci, a free, cross-platform tool for spectroscopic imaging" << endl;
    log_stream_ << "Version 0.4" << endl << endl;
    log_stream_ << "Dataset " << name << "created "
                << datetime.date().toString("yyyy-MM-dd") << "T"
                << datetime.time().toString("hh:mm:ss") << "Z" << endl;
    log_stream_ << "Imported from binary file " << binary_filename << endl << endl;

    non_spatial_ = false;
    //Set up variables unrelated to hyperspectral data:
    map_list_widget_ = main_window->findChild<QListWidget *>("mapsListWidget");
    map_loading_count_ = 0;
    principal_components_calculated_ = false;
    partial_least_squares_calculated_ = false;
    vertex_components_calculated_ = false;
    z_scores_calculated_ = false;
    directory_ = directory;

    mat input_data;
    input_data.load(binary_filename.toStdString());
    int cols = input_data.n_cols;
    int rows = input_data.n_rows;
    int wavelength_size = cols - 2;
    int spatial_size = rows - 1;
    x_.set_size(spatial_size);
    y_.set_size(spatial_size);
    wavelength_.set_size(wavelength_size);
    spectra_.set_size(spatial_size, wavelength_size);

    wavelength_ = input_data(0, span(2, cols));
    x_ = input_data(span(1, rows), 0);
    y_ = input_data(span(1, rows), 1);
    spectra_ = input_data(span(1, rows), span(2, cols));
    main_window_ = main_window;
}




///
/// \brief VespucciDataset::VespucciDataset
/// \param text_filename The filename of the text file from which data is imported
/// \param main_window The main window of the program
/// \param directory The current global working directory
/// \param log_file The log file
/// \param name The name of the dataset displayed to the user
/// \param x_axis_description A description of the spectral abcissa
/// \param y_axis_description A description of the spectral ordinate
/// \param swap_spatial Whether or not y is located in the first column instead of the second (some Horiba spectrometers do this).
/// Main function for processing data from text files to create VespucciDataset objects.
/// Currently written to accept files in "wide" format, will be expanded to deal
/// with different ASCII formats later with conditionals.
VespucciDataset::VespucciDataset(QString text_filename,
                                 MainWindow *main_window,
                                 QString *directory,
                                 QFile *log_file,
                                 QString name,
                                 QString x_axis_description,
                                 QString y_axis_description,
                                 bool swap_spatial,
                                 InputFileFormat format) : log_stream_(log_file)
{
    //open the text file
    QFile inputfile(text_filename);
    inputfile.open(QIODevice::ReadOnly);
    QTextStream inputstream(&inputfile);

    QDateTime datetime = QDateTime::currentDateTimeUtc();
    log_file_ = log_file;

    log_stream_ << "Vespucci, a free, cross-platform tool for spectroscopic imaging" << endl;
    log_stream_ << "Version 0.4" << endl << endl;
    log_stream_ << "Dataset " << name << " created "
                << datetime.date().toString("yyyy-MM-dd") << "T"
                << datetime.time().toString("hh:mm:ss") << "Z" << endl;
    log_stream_ << "Imported from text file " << text_filename << endl << endl;


    non_spatial_ = false;
    //Set up variables unrelated to hyperspectral data:
    map_list_widget_ = main_window->findChild<QListWidget *>("mapsListWidget");
    map_loading_count_ = 0;
    principal_components_calculated_ = false;
    partial_least_squares_calculated_ = false;
    vertex_components_calculated_ = false;
    z_scores_calculated_ = false;
    directory_ = directory;
    flipped_ = swap_spatial;

    QProgressDialog progress("Loading Dataset...", "Cancel", 0, 100, NULL);
    constructor_canceled_ = TextImport::ImportWideText(text_filename,
                                                       spectra_,
                                                       wavelength_,
                                                       x_, y_,
                                                       swap_spatial,
                                                       &progress,
                                                       "\t");

/*
    int i, j;


    inputstream.seek(0);
    QString wavelength_string = inputstream.readLine();

    QStringList wavelength_string_list =
            wavelength_string.split("\t",  QString::SkipEmptyParts);

    int columns = wavelength_string_list.size();
    wavelength_.set_size(columns);

    for(i=0; i<columns; ++i){
        wavelength_(i) = wavelength_string_list.at(i).toDouble();
    }
    i=0;
    j=0;

    QString spectra_string;

    QStringList spectra_string_list;
    QProgressDialog progress("Counting rows...", "Cancel", 0, 100, NULL);
    progress.setWindowTitle("Loading Dataset");
    progress.setWindowModality(Qt::WindowModal);

    int rows = 0;
    while(inputstream.readLine()!=NULL){
        ++rows;
    }
    progress.setValue(1);
    progress.setRange(0,rows+1);

    spectra_.set_size(rows, columns);
    x_.set_size(rows);
    y_.set_size(rows);

    progress.setLabelText("Parsing spectra...");

    inputstream.seek(0);
    inputstream.readLine(); //discard it to advance to next line

    for(i=0; i<rows; ++i){
        spectra_string=inputstream.readLine();
        spectra_string_list =
                spectra_string.split("\t", QString::SkipEmptyParts);

        if (swap_spatial){
            y_(i) = spectra_string_list.at(0).toDouble();
            spectra_string_list.removeAt(0);

            x_(i) = spectra_string_list.at(0).toDouble();
            spectra_string_list.removeAt(0);
        }
        else{
            x_(i) = spectra_string_list.at(0).toDouble();
            spectra_string_list.removeAt(0);

            y_(i) = spectra_string_list.at(0).toDouble();
            spectra_string_list.removeAt(0);
        }

        for (j=0; j<columns; ++j){
            spectra_(i,j) = spectra_string_list.at(j).toDouble();
        }
        if (progress.wasCanceled()){
            constructor_canceled_ = true;
            return;
        }
        progress.setValue(i);
    }

    */

    constructor_canceled_ = false;
    name_ = name;
    x_axis_description_ = x_axis_description;
    y_axis_description_ = y_axis_description;
    main_window_ = main_window;
}

///
/// \brief VespucciDataset::VespucciDataset
/// \param name The name of the dataset displayed to the user
/// \param main_window The main window of the program
/// \param directory The current global working directory
/// \param log_file The log file
/// \param original The dataset from which this dataset is "extracted"
/// \param indices The indices of the spectra in the previous dataset that will form this one
/// This is a constructor to create a new dataset by "extracting" spectra from a
/// another dataset based on values on an image.
VespucciDataset::VespucciDataset(QString name,
                                 MainWindow *main_window,
                                 QString *directory,
                                 QFile *log_file,
                                 QSharedPointer<VespucciDataset> original,
                                 uvec indices) : log_stream_(log_file)

{
    log_file_ = log_file;
    QDateTime datetime = QDateTime::currentDateTimeUtc();
    log_stream_ << "Dataset " << name << "created "
                << datetime.date().toString("yyyy-MM-dd") << "T"
                << datetime.time().toString("hh:mm:ss") << "Z" << endl;
    log_stream_ << "Created from previous dataset " << original->name() << endl;

    non_spatial_ = true;
    map_list_widget_ = main_window->findChild<QListWidget *>("mapsListWidget");
    map_loading_count_ = 0;
    principal_components_calculated_ = false;
    partial_least_squares_calculated_ = false;
    vertex_components_calculated_ = false;
    z_scores_calculated_ = false;
    directory_ = directory;


    spectra_ = original->spectra(indices);
    wavelength_ = original->wavelength();
    x_ = original->x(indices);
    y_ = original->y(indices);
    name_ = name;
    main_window_ = main_window;
    directory_ = directory;
}

///
/// \brief VespucciDataset::VespucciDataset
/// \param name Dataset name
/// \param main_window The
/// \param directory
/// \param log_file
/// Constructor to create a dataset without spatial and spectral data set (i.e.
/// when using MetaDataset).
VespucciDataset::VespucciDataset(QString name,
                                 MainWindow *main_window,
                                 QString *directory,
                                 QFile *log_file) : log_stream_(log_file)
{
    log_file_ = log_file;
    non_spatial_ = true;
    map_list_widget_ = main_window->findChild<QListWidget *>("mapsListWidget");
    map_loading_count_ = 0;
    principal_components_calculated_ = false;
    partial_least_squares_calculated_ = false;
    vertex_components_calculated_ = false;
    z_scores_calculated_ = false;
    directory_ = directory;
    name_ = name;
    main_window_ = main_window;
    directory_ = directory;
}


// PRE-PROCESSING FUNCTIONS //
///
/// \brief VespucciDataset::Undo Swap spectra_ and spectra_old_ to undo an action.
/// Calling this function again re-does the action that was undid.
///
void VespucciDataset::Undo()
{
    log_stream_ << "Undo: " << last_operation_ << endl << endl;
    last_operation_ = "Undo";
    mat buffer = spectra_;
    spectra_ = spectra_old_;
    spectra_old_ = buffer;
}


///
/// \brief VespucciDataset::CropSpectra
/// Crops spectra_ based on
/// \param x_min value of x below which spectra are deleted
/// \param x_max value of x above which spectra are deleted
/// \param y_min value of y below which spectra are deleted
/// \param y_max value of y above which spectra are deleted
/// Removes all data points outside of the range. Cannot be undone. It is preferable
/// to create a new dataset rather than crop an old one.
void VespucciDataset::CropSpectra(double x_min, double x_max, double y_min, double y_max)
{
    log_stream_ << "CropSpectra" << endl;
    log_stream_ << "x_min == " << x_min << endl;
    log_stream_ << "x_max == " << x_max << endl;
    log_stream_ << "y_min == " << y_min << endl;
    log_stream_ << "y_max == " << y_max << endl << endl;


    int max = x_.n_rows;
    QProgressDialog progress("Cropping...", "Cancel", 0, max);
    progress.setWindowModality(Qt::WindowModal);

    for (int i = 0; i < max; ++i){
        if ((x_(i) < x_min) || (x_(i) > x_max) || (y_(i) < y_min) || (y_(i) > y_max)){
            spectra_.shed_row(i);
            x_.shed_row(i);
            y_.shed_row(i);
            --i; //forces repeat of same index after deletion
            --max;
        }
        progress.setValue(i);
    }
    last_operation_ = "crop";
}


///
/// \brief VespucciDataset::MinMaxNormalize
///normalizes data so that smallest value is 0 and highest is 1 through the
/// entire spectra_ matrix.  If the minimum of spectra_ is negative, it subtracts
/// this minimum from all points.  The entire spectra_ matrix is then divided
/// by the maximum of spectra_
void VespucciDataset::MinMaxNormalize()
{
    log_stream_ << "MinMaxNormalize" << endl << endl;
    spectra_old_ = spectra_;
    int n_elem = spectra_.n_elem;
    double minimum = spectra_.min();
    if (minimum < 0)
        for (int i = 0; i < n_elem; ++i)
            spectra_(i) = spectra_(i) - minimum;
    double maximum = spectra_.max();
    spectra_ = spectra_/maximum;
    last_operation_ = "min/max normalize";
}

///
/// \brief VespucciDataset::UnitAreaNormalize
///normalizes the spectral data so that the area under each point spectrum is 1
void VespucciDataset::UnitAreaNormalize()
{
    log_stream_ << "UnitAreaNormalize" << endl << endl;
    spectra_old_ = spectra_;
    uword num_rows = spectra_.n_rows;
    uword num_cols = spectra_.n_cols;
    for (uword i = 0; i < num_rows; ++i){
        rowvec row = spectra_.row(i);
        double row_sum = sum(row);
        for (uword j = 0; j < num_cols; ++j){
            spectra_(i, j) = spectra_(i, j) / row_sum;
        }
    }
    last_operation_ = "unit area normalize";
}

///
/// \brief VespucciDataset::ZScoreNormCopy
/// For when you want to Z-score normalize without changing spectra_
/// \return A normalized copy of the matrix.
///
mat VespucciDataset::ZScoreNormCopy()
{
    uword num_rows = spectra_.n_rows;
    uword num_cols = spectra_.n_cols;
    mat normalized_copy(num_rows, num_cols);
    for (uword j = 0; j < num_cols; ++j){
        double mean = arma::mean(spectra_.col(j));
        double standard_deviation = arma::stddev(spectra_.col(j));
        for (uword i = 0; i < num_rows; ++i){
            normalized_copy(i, j) = (spectra_(i, j) - mean) / standard_deviation;
        }
    }
    return normalized_copy;
}

///
/// \brief VespucciDataset::ZScoreNormalize
/// Computes a Z score for every entry based on the distribution of its column,
/// assuming normality of "population".  Because some values will be negative,
/// this must be accounted for in Univariate Mapping Functions. Keep in mind that
/// data is pre-centered by row for all methods (PCA, PLS, etc) that require
/// centered data, but not necessarily by column, as it is here.
///
void VespucciDataset::ZScoreNormalize()
{
    log_stream_ << "ZScoreNormalize" << endl;
    spectra_old_ = spectra_;
    uword num_rows = spectra_.n_rows;
    uword num_cols = spectra_.n_cols;
    for (uword j = 0; j < num_cols; ++j){
        double mean = arma::mean(spectra_.col(j));
        double standard_deviation = arma::stddev(spectra_.col(j));
        for (uword i = 0; i < num_rows; ++i){
            spectra_(i, j) = (spectra_(i, j) - mean) / standard_deviation;
        }
    }
    z_scores_calculated_ = true;
    last_operation_ = "Z-score normalize";

}

///
/// \brief VespucciDataset::SubtractBackground
/// Subtracts a known background spectrum. This can be extracted from a control
/// map using this software (using * component analysis endmember extraction or
/// average spectrum).
/// \param background A matrix consisting of a single spectrum representing the
/// background.
///
void VespucciDataset::SubtractBackground(mat background, QString filename)
{
    log_stream_ << "SubtractBackground" << endl;
    log_stream_ << "filename == " << filename << endl << endl;
    spectra_old_ = spectra_;
    if (background.n_cols != spectra_.n_cols){
        QMessageBox::warning(0,
                             "Improper Dimensions!",
                             "The background spectrum has a different number of"
                             " points than the map data."
                             " No subtraction can be performed");
        return;
    }
    else{
        spectra_.each_row() -= background.row(0);
    }
    last_operation_ = "background correction";
}

///
/// \brief VespucciDataset::Baseline
/// Baseline-adjusts the data. This function uses a median filter with a large
/// window to determine the baseline on the assumption that the median value
/// is more likely to be basline than spectrum. This will complicate things if
/// there are many peaks. Additionally, this significantly narrows the shape of
/// peaks. Other baseline methods may be implemented later.
/// \param method
/// \param window_size
///
void VespucciDataset::Baseline(QString method, int window_size)
{
    log_stream_ << "Baseline" << endl;
    log_stream_ << "method == " << method << endl;
    log_stream_ << "window_size == " << window_size << endl << endl;
    spectra_old_ = spectra_;
    if (method == "Median Filter"){
        uword starting_index = (window_size - 1) / 2;
        uword ending_index = wavelength_.n_cols - starting_index;
        uword i, j;
        uword rows = spectra_.n_rows;
        uword columns = spectra_.n_cols;
        rowvec window;
        mat processed;
        window.set_size(window_size);
        processed.set_size(spectra_.n_rows, spectra_.n_cols);

        for (i = 0; i < rows; ++i){
            for (j = 0; j < starting_index; ++j){
                processed(i, j) = spectra_(i, j);
            }
            for (j = ending_index; j < columns; ++j){
                processed(i, j) = spectra_(i, j);
            }
            for (j = starting_index; j < ending_index; ++j){
                window = spectra_(i, span((j - starting_index), (j+starting_index)));
                processed(i, j) = median(window);
            }
        }
        spectra_ -= processed;
    }
    last_operation_ = "baseline correction";
}

//Filtering functions
///
/// \brief VespucciDataset::MedianFilter
/// performs median filtering on the spectral data.  Entries near the boundaries
/// of spectra are not processed. See also VespucciDataset::LinearMovingAverage
/// \param window_size - an odd number representing the width of the window.

void VespucciDataset::MedianFilter(unsigned int window_size)
{
    log_stream_ << "MedianFilter" << endl;
    log_stream_ << "window_size == " << window_size << endl << endl;

    spectra_old_ = spectra_;
    uword starting_index = (window_size - 1) / 2;
    uword ending_index = wavelength_.n_cols - starting_index;
    uword i, j;
    uword rows = spectra_.n_rows;
    uword columns = spectra_.n_cols;
    rowvec window;
    mat processed;
    window.set_size(window_size);
    processed.set_size(spectra_.n_rows, spectra_.n_cols);

    for (i = 0; i < rows; ++i){
        for (j = 0; j < starting_index; ++j){
            processed(i, j) = spectra_(i, j);
        }
        for (j = ending_index; j < columns; ++j){
            processed(i, j) = spectra_(i, j);
        }
        for (j = starting_index; j < ending_index; ++j){
            window = spectra_(i, span((j - starting_index), (j+starting_index)));
            processed(i, j) = median(window);
        }
    }
    spectra_ = processed;
    last_operation_ = "median filter";
}

///
/// \brief VespucciDataset::LinearMovingAverage
/// Performs moving average filtering on the spectral data.  Entries near the
/// boundaries of spectra are not processed.  See also VespucciDataset::MedianFilter.
/// \param window_size - an odd number representing the width of the window.

void VespucciDataset::LinearMovingAverage(unsigned int window_size)
{
    log_stream_ << "LinearMovingAverage" << endl;
    log_stream_ << "window_size == " << window_size << endl << endl;

    spectra_old_ = spectra_;
    vec filter = arma_ext::CreateMovingAverageFilter(window_size);
    //because armadillo is column-major, it is faster to filter by columns than rows
    mat buffer = trans(spectra_);
    mat filtered(buffer.n_rows, buffer.n_cols);
    for (uword j = 0; j < buffer.n_cols; ++j){
        filtered.col(j) = arma_ext::ApplyFilter(buffer.col(j), filter);
    }
    spectra_ = trans(filtered);
    last_operation_ = "moving average filter";
}

///
/// \brief VespucciDataset::SingularValue
/// Denoises the spectra matrix using a truncated singular value decomposition.
/// The first singular_values singular values are used to "reconstruct" the
/// spectra matrix. The function used to find the truncated SVD is
/// arma_ext::svds.
/// \param singular_values Number of singular values to use.
///
void VespucciDataset::SingularValue(unsigned int singular_values)
{
    log_stream_ << "SingularValue" << endl;
    log_stream_ << "singular_values == " << singular_values << endl << endl;
    spectra_old_ = spectra_;
    mat U;
    vec s;
    mat V;
    arma_ext::svds(spectra_, singular_values, U, s, V);
    spectra_ = -1 * U * diagmat(s) * V.t();
    last_operation_ = "truncated SVD de-noise";
}

///
/// \brief VespucciDataset::Derivatize
/// Performs derivatization/Savitzky-Golay smoothing
/// \param derivative_order The order of the derivative.
/// \param polynomial_order The order of the polynomial
/// \param window_size The size of the filter window.
///
void VespucciDataset::Derivatize(unsigned int derivative_order,
                                 unsigned int polynomial_order,
                                 unsigned int window_size)
{
    log_stream_ << "Derivatize (Savitzky-Golay Smoothing)" << endl;
    log_stream_ << "derivative_order == " << derivative_order << endl;
    log_stream_ << "polynomial_order == " << polynomial_order << endl;
    log_stream_ << "window_size == " << window_size << endl << endl;
    spectra_old_ = spectra_;
    mat temp = arma_ext::sgolayfilt(trans(spectra_),
                                      polynomial_order,
                                      window_size,
                                      derivative_order,
                                      1);
    spectra_ = trans(temp);
    last_operation_ = "Savitzky-Golay filtering";
}

// MAPPING FUNCTIONS //

///
/// \brief VespucciDataset::Univariate
/// Creates a univariate image. Several peak-determination methods are availible.
/// All methods except for "Intensity" estimate a local baseline. This is done
/// by drawing a straight line from the left to right endpoint. This can cause
/// problems when the endpoints are near other local maxima.
///
/// The "Bandwidth" method calculates the full-width at half maximum of the peak
/// near the range specified.
///
/// The "Intensity" method calculates the local maximum of the spectrum within
/// the range specified.
///
/// The "Area" method takes a right Riemann sum of the spectrum
///
/// The "Derivative" method is misleadingly named (this is based on in-house
/// MATLAB code previously used by my group). The derivative method is actually
/// and area method which finds the edges of the peak by taking a second derivative.
/// It then determines the peak in an identical fashion to the "Area" method///
///
/// \param min left bound of spectral region of interest
/// \param max right bound of spectral region of interest
/// \param name name of MapData object to be created
/// \param value_method method of determining peak (intensity, derivative, or area)
/// \param gradient_index index of color scheme in master list (GetGradient());
///
void VespucciDataset::Univariate(uword min,
                                 uword max,
                                 QString name,
                                 QString value_method,
                                 QString integration_method,
                                 uword gradient_index)
{
    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    uword size = x_.n_elem;
    uword i;

    log_stream_ << "Univariate" << endl;
    log_stream_ << "min == " << min << endl;
    log_stream_ << "max == " << max << endl;
    log_stream_ << "name == " << name << endl;
    log_stream_ << "value_method == " << value_method << endl;
    log_stream_ << "integration_method == " << integration_method << endl;
    log_stream_ << "gradient_index == " << gradient_index << endl;


    rowvec region;
    colvec results;
    results.set_size(size);
    QString map_type;
    rowvec abcissa;
    mat baselines;
    mat mid_lines;

    if (value_method == "Bandwidth"){
        double maximum, half_maximum, width/*, region_size*/;
        double start_value, end_value, slope;
        baselines.set_size(size, max-min + 1);
        uword max_index = 0;
        uword left_index = 0;
        uword right_index = 0;
        map_type = "1-Region Univariate (Bandwidth (FWHM))";
        uword columns = spectra_.n_cols;
        abcissa.set_size(max-min + 1);
        abcissa = wavelength_.subvec(span(min, max));
        mid_lines.set_size(x_.n_rows, 4);
        for (i = 0; i < size; ++i){

            start_value = spectra_(i, min);
            end_value = spectra_(i, max);
            slope = (end_value - start_value) / (max - min);
            uword j;
            for (j = 0; j <= (max - min); ++j)
                baselines(i, j) = j*slope + start_value;


            //find maximum and half-maximum
            region = spectra_(i, span(min, max));
            //region_size = region.n_elem;
            maximum = region.max();

            //find index of maximum
            for (j = 0; j < columns; ++j){
                if (maximum == spectra_(i, j) && j >= min && j <= max){
                    max_index = j;
                    break;
                }
            }

            int local_max_index = max_index-min;
            half_maximum = (maximum - baselines(i, local_max_index) / 2.0) +
                    baselines(i, local_max_index);

            //find index of left limit
            for (j = max_index; j > 0; --j){
                if (spectra_(i, j) - half_maximum < 0){
                    left_index = j;
                    break;
                }
            }

            //find index of right limit
            for (j = max_index; j < columns; ++j){
                if (spectra_(i, j) - half_maximum < 0){
                    right_index = j;
                    break;
                }
            }

            //make sure adjacent points on other side of inflection aren't better
            if (fabs(spectra_(i, left_index) - half_maximum) <
                    fabs(spectra_(i, left_index - 1) - half_maximum)){
                --left_index;
            }
            if (fabs(spectra_(i, right_index) - half_maximum) <
                     fabs(spectra_(i, right_index + 1) - half_maximum)){
                ++right_index;
            }

            //record to results.  using fabs because order of wavelength unknown
            width = fabs(wavelength_(right_index) - wavelength_(left_index));
            results(i) = width;
            mid_lines(i, 0) = wavelength_(left_index);
            mid_lines(i, 1) = spectra_(left_index);
            mid_lines(i, 2) = wavelength_(right_index);
            mid_lines(i, 3) = spectra_(right_index);
        }

    }

    else if(value_method == "Area"){
        // Do peak fitting stuff here.
        map_type = "1-Region Univariate (Area)";
        abcissa.set_size(max - min + 1);
        abcissa = wavelength_.subvec(span(min, max));
        if (integration_method == "Riemann Sum"){
            double start_value, end_value, slope;
            baselines.set_size(size, abcissa.n_cols);

            for (i=0; i<size; ++i){
                start_value = spectra_(i, min);
                end_value = spectra_(i, max);
                slope = (end_value - start_value) / (max - min);
                uword j;
                for (j = 0; j <= (max - min); ++j)
                    baselines(i, j) = j*slope + start_value;

                region = spectra_(i, span(min, max));
                region -= baselines.row(i);
                results(i) = sum(region);
            }
        }
    }

    else if(value_method == "Derivative"){
        // Do derivative stuff here
        map_type = "1-Region Univariate (Derivative)";
    }

    else{
        // Makes an intensity map
        map_type = "1-Region Univariate (Intensity)";
        if (z_scores_calculated_){
            uword elements = spectra_.n_elem;
            rowvec region_temp;
            double peak_height;
            double peak_height_temp;
            mat spectra_temp(spectra_.n_rows, spectra_.n_cols);
            for (i = 0; i < elements; ++i)
                spectra_temp(i) = fabs(spectra_(i));

            for (i = 0; i < size; ++i){
                region = spectra_(i, span(min, max));
                region_temp = spectra_temp(i, span(min, max));
                peak_height_temp = region_temp.max();
                peak_height = region.max();

                // If the maxes aren't equal, then we know the peak is negative
                if (peak_height_temp != peak_height)
                    peak_height = peak_height_temp * -1.0;

                results(i) = peak_height;
            }

        }
        else{
            for (i=0; i < size; ++i){
                region = spectra_(i, span(min, max));
                results(i)=region.max();
            }

        }

    }

    QSharedPointer<MapData> map(new MapData(x_axis_description_,
                                            y_axis_description_,
                                            x_, y_, results,
                                            QSharedPointer<VespucciDataset>(this),
                                            directory_,
                                            this->GetGradient(gradient_index),
                                            maps_.size(),
                                            6,
                                            main_window_));


    map->set_name(name, map_type);

    if(baselines.n_rows !=0){
        map->set_baseline(abcissa, baselines);
    }

    if(mid_lines.n_rows != 0){
        map->set_fwhm(mid_lines);
    }

    this->AddMap(map);
    maps_.last()->ShowMapWindow();
}

///
/// \brief VespucciDataset::BandRatio
/// Creates a band ratio univariate map. Band ratio maps represent the ratio of
/// two peaks. The determination methods here are identical to those in
/// VespucciDataset::Univariate.
/// \param first_min index of left bound of first region of interest
/// \param first_max index of right bound of first region of interest
/// \param second_min index of left bound of second region of interest
/// \param second_max index of right bound of second region of interest
/// \param name name of the MapData object to be created.  This becomes name of the map to the user
/// \param value_method how the maxima are to be determined (area, derivative, or intensity)
/// \param gradient_index index of gradient in the master list (GetGradient())
///
void VespucciDataset::BandRatio(uword first_min,
                        uword first_max,
                        uword second_min,
                        uword second_max,
                        QString name,
                        QString value_method,
                        QString integration_method,
                        unsigned int gradient_index)
{

    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    QString map_type;

    log_stream_ << "BandRatio" << endl;
    log_stream_ << "first_min == " << first_min << endl;
    log_stream_ << "first_max == " << first_max << endl;
    log_stream_ << "second_min == " << second_min << endl;
    log_stream_ << "second_max == " << second_max << endl;
    log_stream_ << "name == " << name << endl;
    log_stream_ << "value_method == " << value_method << endl;
    log_stream_ << "integration_method == " << integration_method << endl;
    log_stream_ << "gradient_index == " << gradient_index << endl << endl;

    uword size = x_.n_elem;
    uword i;

    rowvec first_region;
    rowvec second_region;
    colvec results;

    rowvec first_abcissa;
    rowvec second_abcissa;
    mat first_baselines;
    mat second_baselines;

    results.set_size(size);

    if(value_method == "Area"){
        // Do peak fitting stuff here.
        map_type = "2-Region Band Ratio Map (Area)";
        if (integration_method == "Riemann Sum"){
            double first_start_value, first_end_value, second_start_value,
                    second_end_value, first_slope, second_slope, first_sum,
                    second_sum;
            first_abcissa.set_size(first_max - first_min + 1);
            second_abcissa.set_size(second_max - second_min + 1);
            first_abcissa = wavelength_.subvec(span(first_min, first_max));
            second_abcissa = wavelength_.subvec(span(second_min, second_max));\
            first_baselines.set_size(size, first_max - first_min + 1);
            second_baselines.set_size(size, second_max - second_min + 1);

            for (i=0; i<size; ++i){
                first_start_value = spectra_(i, first_min);
                second_start_value = spectra_(i, second_min);
                first_end_value = spectra_(i, first_max);
                second_end_value = spectra_(i, second_max);
                first_slope = (first_end_value - first_start_value) / (first_max - first_min);
                second_slope = (second_end_value - second_start_value) / (second_max - second_min);
                uword j;
                for (j = 0; j <= (first_max - first_min); ++j)
                    first_baselines(i, j) = j*first_slope + first_start_value;
                for (j = 0; j <= (second_max - second_min); ++j)
                    second_baselines(i, j) = j*second_slope + second_start_value;

                first_region = spectra_(i, span(first_min, first_max));
                second_region = spectra_(i, span(second_min, second_max));

                first_sum = sum(first_region - first_baselines.row(i));
                second_sum = sum(second_region - second_baselines.row(i));

                results(i)= first_sum / second_sum;
            }

        }


    }

    else if(value_method == "Derivative"){
        // Do derivative stuff here
        map_type = "2-Region Band Ratio Map (Derivative)";
    }

    else{
        // Makes an intensity map
        map_type = "2-Region Band Ratio Map (Intensity)";
        for (i=0; i<size; ++i){
            first_region = spectra_(i, span(first_min, first_max));
            second_region = spectra_(i, span(second_min, second_max));
            results(i)= first_region.max()/second_region.max();
        }
    }

    QSharedPointer<MapData> new_map(new MapData(x_axis_description_,
                                            y_axis_description_,
                                            x_, y_, results,
                                            QSharedPointer<VespucciDataset>(this), directory_,
                                            this->GetGradient(gradient_index),
                                            maps_.size(),
                                            6,
                                            main_window_));


    new_map->set_name(name, map_type);
    if (first_baselines.n_rows != 0){
        new_map->set_baselines(first_abcissa, second_abcissa, first_baselines, second_baselines);
    }
    this->AddMap(new_map);
    maps_.last()->ShowMapWindow();
}



///
/// \brief VespucciDataset::PrincipalComponents
/// Performs principal component analysis on the data.  Uses armadillo's pca routine.
/// This function both calculates and plots principal components maps.
/// \param component the PCA component from which the image will be produced
/// \param name the name of the MapData object to be created
/// \param gradient_index the index of the gradient in the master list (in function GetGradient)
///
void VespucciDataset::PrincipalComponents(int component,
                                  QString name,
                                  int gradient_index, bool recalculate)
{
    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    log_stream_ << "PrincipalComponents" << endl;
    log_stream_ << "component == " << component << endl;
    log_stream_ << "name == " << name << endl;
    log_stream_ << "gradient_index == " << gradient_index << endl;
    log_stream_ << "recalculate == " << (recalculate ? "true" : "false") << endl << endl;

    if (recalculate || !principal_components_calculated_){

        component--;

        QMessageBox alert;
        alert.setText("Calculating principal components may take a while.");
        alert.setInformativeText("When complete, the image will appear in a new window. "
                                 "The program may appear not to respond.  Principal "
                                 "components only need to be calculated once per dataset. "
                                 "OK to continue");

        alert.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        alert.setWindowTitle("Principal Components Analysis");
        alert.setIcon(QMessageBox::Question);

        int ret = alert.exec();


        if (ret == QMessageBox::Cancel){
            return;
        }

        if (ret == QMessageBox::Ok){


            cout << "call to arma::princomp" << endl;
            wall_clock timer;
            timer.tic();
            principal_components_data_ = new PrincipalComponentsData(QSharedPointer<VespucciDataset>(this),
                                                                     directory_);
            principal_components_data_->Apply(spectra_);
            int seconds = timer.toc();
            cout << "call to arma::princomp successful. Took " << seconds << " s" << endl;
            principal_components_calculated_ = true;
        }
    }

    QString map_type;
    QTextStream(&map_type) << "(Principal Component " << component + 1 << ")";

    colvec results = principal_components_data_->Results(component);

    QSharedPointer<MapData> new_map(new MapData(x_axis_description_,
                                            y_axis_description_,
                                            x_, y_, results,
                                            QSharedPointer<VespucciDataset>(this), directory_,
                                            GetGradient(gradient_index),
                                            maps_.size(),
                                            6,
                                            main_window_));
    new_map->set_name(name, map_type);
    AddMap(new_map);
    maps_.last()->ShowMapWindow();
}


///
/// \brief VespucciDataset::VertexComponents
/// \param endmembers
/// \param image_component
/// \param name
/// \param gradient_index
/// \param recalculate
///
void VespucciDataset::VertexComponents(uword endmembers,
                               uword image_component,
                               QString name,
                               unsigned int gradient_index,
                               bool recalculate)
{
    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    log_stream_ << "VertexComponents" << endl;
    log_stream_ << "endmembers == " << endmembers << endl;
    log_stream_ << "image_component == " << image_component << endl;
    log_stream_ << "gradient_index == " << gradient_index;
    log_stream_ << "recalculate == " << (recalculate ? "true" : "false") << endl << endl;

    QString map_type;
    QTextStream(&map_type) << "(Vertex Component " << image_component << ")";
    if (recalculate || !vertex_components_calculated_){
        vertex_components_data_ = new VCAData(QSharedPointer<VespucciDataset>(this), directory_);
        vertex_components_data_->Apply(spectra_, endmembers);
        vertex_components_calculated_ = true;
    }
    colvec results = vertex_components_data_->Results(image_component-1);

    //assume all negative values are actually 0

    QSharedPointer<MapData> new_map(new MapData(x_axis_description_,
                                                y_axis_description_,
                                                x_, y_, results,
                                                QSharedPointer<VespucciDataset>(this), directory_,
                                                GetGradient(gradient_index),
                                                maps_.size(),
                                                6,
                                                main_window_));
    new_map->set_name(name, map_type);
    AddMap(new_map);
    maps_.last()->ShowMapWindow();
}

///
/// \brief VespucciDataset::PartialLeastSquares
/// Performs PLS regression on data.  Resulting map is score for one PLS Component,
/// taken from one column of the X loadings.
/// PLS is performed once.  Subsequent maps use data from first call, stored
/// as PartialLeastSquaresData object, unless user requests recalculation.
/// \param components the number of components/response variables of the regression data
/// \param name the name of the MapData object to be created.
/// \param gradient_index the index of the color gradient in the color gradient list
/// \param recalculate whether or not to recalculate PLS regression.
///
void VespucciDataset::PartialLeastSquares(uword components,
                                  uword image_component,
                                  QString name,
                                  unsigned int gradient_index,
                                  bool recalculate)
{
    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    log_stream_ << "PartialLeastSqares" << endl;
    log_stream_ << "components == " << components << endl;
    log_stream_ << "image_component == " << image_component << endl;
    log_stream_ << "name == " << name << endl;
    log_stream_ << "gradient_index == " << gradient_index << endl;
    log_stream_ << "recalculate == " << (recalculate ? "true" : "false") << endl << endl;


    image_component--;
    QString map_type = "Partial Least Squares Map number of components = ";

    if (recalculate || !partial_least_squares_calculated_){
        map_type += QString::number(components);
        partial_least_squares_data_ = new PLSData(QSharedPointer<VespucciDataset>(this), directory_);
        bool success = partial_least_squares_data_->Apply(spectra_, wavelength_, components);
        if (success){
            partial_least_squares_calculated_ = true;
        }
    }


    bool valid;
    colvec results = partial_least_squares_data_->Results(image_component, valid);
    if (!valid){
        QMessageBox::warning(main_window_, "Index out of Bounds",
                             "The component number requested is greater than"
                             "the number of components calculated. Map generated"
                             "corresponds to the highest component number"
                             "calculated");
    }


    map_type += QString::number(partial_least_squares_data_->NumberComponents());
    map_type += ". Component number " + QString::number(image_component);

    QSharedPointer<MapData> new_map(new MapData(x_axis_description_,
                                            y_axis_description_,
                                            x_, y_, results,
                                            QSharedPointer<VespucciDataset>(this), directory_,
                                            this->GetGradient(gradient_index),
                                            maps_.size(),
                                            6,
                                            main_window_));
    new_map->set_name(name, map_type);
    this->AddMap(new_map);
    maps_.last()->ShowMapWindow();
}

///
/// \brief VespucciDataset::KMeans
/// Implements K-means clustering using MLPACK
/// \param clusters Number of clusters to find
/// \param name Name of map in workspace.
///
void VespucciDataset::KMeans(size_t clusters, QString name)
{
    //if dataset is non spatial, just quit
    if(non_spatial_){
        QMessageBox::warning(0, "Non-spatial dataset", "Dataset is non-spatial or non-contiguous! Mapping functions are not available");
        return;
    }
    log_stream_ << "KMeans" << endl;
    log_stream_ << "clusters == " << clusters << endl;
    log_stream_ << "name == " << name << endl << endl;

    QString map_type = "K-means clustering map. Number of clusters = ";
    map_type += QString::number(clusters);
    Col<size_t> assignments;
    mlpack::kmeans::KMeans<> k;
    mat data = trans(spectra_);
    k.Cluster(data, clusters, assignments);
    k_means_data_.set_size(assignments.n_elem, 1);
    k_means_calculated_ = true;
    //loop for copying values, adds one so clusters indexed at 1, not 0.
    for (uword i = 0; i < assignments.n_rows; ++i)
        k_means_data_(i, 0) = assignments(i) + 1;

    QCPColorGradient gradient = GetClusterGradient(clusters);
    QSharedPointer<MapData> new_map(new MapData(x_axis_description_,
                                            y_axis_description_,
                                            x_, y_, k_means_data_.col(0),
                                            QSharedPointer<VespucciDataset>(this), directory_,
                                            gradient,
                                            maps_.size(),
                                            clusters,
                                            main_window_));
    new_map->set_name(name, map_type);
    new_map->SetCrispClusters(true);
    this->AddMap(new_map);
    maps_.last()->ShowMapWindow();
}


// HELPER FUNCTIONS //

///
/// \brief VespucciDataset::FindRange.
/// Finds the index of the wavelength value closest
/// to the specified wavelength range.
/// \param start the first wavelength in the spectral region of interest
/// \param end the second wavelength in the spectral region of interest
/// \return
///

uvec VespucciDataset::FindRange(double start, double end)
{
    uvec indices(2);
    uword length = wavelength_.size();
    uword i;
    for (i=0; i<length; ++i){
        if(wavelength_(i)>=start){
            break;
        }
    }
    if (i==length-1){
        cerr << "Can't find index of start of range!" << endl;
        return indices; //in this case, indices contains only zeroes
    }

    if (fabs(wavelength_(i)-start)>fabs(wavelength_(i-1)-start)){
        indices(0)=i-1;
    }

    else{
        indices(0)= i;
    }
    uword it = i;
    for (i = it; i < length; ++i){
        if(wavelength_(i)>=end){
            break;
        }
    }

    if(i==length-1){
        cerr << "Can't find index of upper wavelength limit!" << endl;
        cerr << "Setting upper limit equal to lower limit (point ROI)" << endl;
        indices[1]=indices[0];
        return indices;
    }

    if (fabs(wavelength_(i)-start)>fabs(wavelength_(i-1)-start)){
        indices[1] = i-1;
    }

    else{
        indices[1] = i;
    }
    return indices;
}

/// \brief VespucciDataset::PointSpectrum
/// \param index
/// \return
///
QVector<double> VespucciDataset::PointSpectrum(const uword index)
{
    //perform bounds check.
    std::vector<double> spectrum_stdvector;
    if (index > spectra_.n_rows){
        spectrum_stdvector =
                conv_to< std::vector<double> >::from(spectra_.row(spectra_.n_rows - 1));
    }
    else{
         spectrum_stdvector =
                 conv_to< std::vector<double> >::from(spectra_.row(index));
    }

    QVector<double> spectrum_qvector =
            QVector<double>::fromStdVector(spectrum_stdvector);

    return spectrum_qvector;
}

QVector<double> VespucciDataset::WavelengthQVector()
{
    std::vector<double> wavelength_stdvector =
            conv_to< std::vector<double> >::from(wavelength_);

    QVector<double> wavelength_qvector =
            QVector<double>::fromStdVector(wavelength_stdvector);

    return wavelength_qvector;
}

///
/// \brief VespucciDataset::ValueRange
/// Finds the minima and maxima of y variable to properly set axes
///  of QCustomPlot objects
/// \return
///
QCPRange VespucciDataset::ValueRange()
{
    double lower = y_.min();
    double upper = y_.max();
    QCPRange range(upper, lower);
    return range;
}
///
/// \brief VespucciDataset::KeyRange
/// Finds the minima and maxima of x variable to properly set axes
///  of QCustomPlot objects
/// \return
///
QCPRange VespucciDataset::KeyRange()
{
    double lower = x_.min();
    double upper = x_.max();
    QCPRange range(upper, lower);
    return range;
}

///
/// \brief VespucciDataset::KeySize
/// Finds the number of data points in x variable to properly set axes
///  of QCustomPlot objects
/// \return number of unique x values
///
int VespucciDataset::KeySize()
{
    uword i;
    uword x_count=1;
    double x_buf;

    //loop through x until a value different then the first is met.
    if (!flipped_){
        x_count = 1; //this counts the first entry in x_
        x_buf = x_(0);
        for(i=0; i<x_.n_elem; ++i){
            if(x_(i)!=x_buf){
                ++x_count;
                x_buf=x_(i);
            }
        }
    } else{
        x_count = 0;
        for (i=0; i<x_.n_elem; ++i){
            if(y_(i)!=y_(0)){
                break;
            } else{
                ++x_count;
            }
        }
    }

    return x_count;
}

///
/// \brief VespucciDataset::ValueSize
/// Finds number of unique y values for properly setting QCPAxis
/// \return number of unique y values
///
int VespucciDataset::ValueSize()
{

    uword i = 0;
    uword y_count;


    //long-text files hold x constant and vary y
    //until x is different, count y
    //reverse if flipped
    if (!flipped_){
        y_count = 0;
        for (i=0; i<x_.n_elem; ++i){
            if(x_(i)!=x_(0)){
                break;
            }
            else{
                ++y_count;
            }
        }
    } else{
        y_count = 1;
        double y_buf = y_(0);
        for(i=0; i<y_.n_elem; ++i){
            if(y_(i)!=y_buf){
                ++y_count;
                y_buf=y_(i);
            }
        }
    }


    return y_count;
}


// MEMBER ACCESS FUNCTIONS //
///
/// \brief VespucciDataset::wavelength
/// \return member wavelength_ (spectrum key values)
///
rowvec VespucciDataset::wavelength()
{
    return wavelength_;
}

rowvec VespucciDataset::wavelength(uvec indices)
{
    return wavelength_.cols(indices);
}

///
/// \brief VespucciDataset::x
/// \return member x_
///
colvec VespucciDataset::x()
{
    return x_;
}

///
/// \brief VespucciDataset::x
/// \param indices Vector of indices
/// \return Subvec of x corresponding to valeus in indices
///
colvec VespucciDataset::x(uvec indices)
{
    return x_(indices);
}

double VespucciDataset::x(uword index)
{
    if (index >= x_.n_rows)
        return x_(x_.n_rows - 1);
    else
        return x_(index);
}



///
/// \brief VespucciDataset::y
/// \return member y_
///
colvec VespucciDataset::y()
{
    return y_;
}

///
/// \brief VespucciDataset::y
/// \param indices Vector of indices
/// \return Subvec of y at indices
///
colvec VespucciDataset::y(uvec indices)
{
    return y_(indices);
}

double VespucciDataset::y(uword index)
{
    if (index >= y_.n_rows)
        return y_(y_.n_rows - 1);
    else
        return y_(index);
}

///
/// \brief VespucciDataset::spectra
/// \return member spectra_
///
mat VespucciDataset::spectra()
{
    return spectra_;
}

///
/// \brief VespucciDataset::spectra
/// \param indices Vector of indices
/// \return Submat of spectra at indices
///
mat VespucciDataset::spectra(uvec indices)
{
    return spectra_.rows(indices);
}

///
/// \brief VespucciDataset::name
/// \return member name_, the name of the dataset as seen by the user
///
const QString VespucciDataset::name()
{
    return name_;
}

///
/// \brief VespucciDataset::SetName
/// \param new_name new name of dataset
///
void VespucciDataset::SetName(QString new_name)
{
    name_ = new_name;
}

///
/// \brief VespucciDataset::SetData
/// \param spectra Spectra
/// \param wavelength Spectral abcissa
/// \param x Colvec of horizontal axis spatial data
/// \param y Colvec of vertical axis spatial data
/// Set the data of the dataset. Used by the MetaDataset constructor
void VespucciDataset::SetData(mat spectra, rowvec wavelength, colvec x, colvec y)
{
    spectra_ = spectra;
    wavelength_ = wavelength;
    x_ = x;
    y_ = y;
}

//MAP HANDLING FUNCTIONS
///
/// \brief VespucciDataset::map_names
/// \return list of names of the maps created from this dataset
///
QStringList VespucciDataset::map_names()
{
    return map_names_;
}

///
/// \brief VespucciDataset::map_loading_count
/// \return number of maps created for this dataset
///
int VespucciDataset::map_loading_count()
{
    return map_loading_count_;
}

///
/// \brief VespucciDataset::RemoveMapAt
/// \param i index of map in the relevant lists
///
void VespucciDataset::RemoveMapAt(unsigned int i)
{
    QListWidgetItem *item = map_list_widget_->takeItem(i);
    map_list_widget_->removeItemWidget(item);
    maps_.removeAt(i); //map falls out of scope and memory freed!

}


///
/// \brief VespucciDataset::AddMap
/// Adds a map to the list of map pointers and adds its name to relevant lists
/// \param map
///
void VespucciDataset::AddMap(QSharedPointer<MapData> map)
{
    QString name = map->name();
    maps_.append(map);
    map_names_.append(name);

    map_list_widget_->addItem(name);
    ++map_loading_count_;
}

///
/// \brief VespucciDataset::WavelengthRange
/// \return the range of the wavlength vector (for plotting point spectra)
///
QCPRange VespucciDataset::WavelengthRange()
{
    double min = wavelength_.min();
    double max = wavelength_.max();
    QCPRange range(min, max);
    return range;
}

///
/// \brief VespucciDataset::PointSpectrumRange
/// \param i row of spectra_ containing desired spectrum
/// \return the range of y values for the point spectra at i
///
QCPRange VespucciDataset::PointSpectrumRange(int i)
{
    rowvec row = spectra_.row(i);
    double min = row.min();
    double max = row.max();

    QCPRange range(min, max);
    return range;
}

///
/// \brief VespucciDataset::GetGradient
/// Selects the color gradient from list of presets
/// \param gradient_number
/// \return
///
QCPColorGradient VespucciDataset::GetGradient(int gradient_number)
{
    switch (gradient_number)
    {
    case 0: return QCPColorGradient::cbBuGn;
    case 1: return QCPColorGradient::cbBuPu;
    case 2: return QCPColorGradient::cbGnBu;
    case 3: return QCPColorGradient::cbOrRd;
    case 4: return QCPColorGradient::cbPuBu;
    case 5: return QCPColorGradient::cbPuBuGn;
    case 6: return QCPColorGradient::cbPuRd;
    case 7: return QCPColorGradient::cbRdPu;
    case 8: return QCPColorGradient::cbYlGn;
    case 9: return QCPColorGradient::cbYlGnBu;
    case 10: return QCPColorGradient::cbYlOrBr;
    case 11: return QCPColorGradient::cbYlOrRd;
    case 12: return QCPColorGradient::cbBlues;
    case 13: return QCPColorGradient::cbGreens;
    case 14: return QCPColorGradient::cbOranges;
    case 15: return QCPColorGradient::cbPurples;
    case 16: return QCPColorGradient::cbReds;
    case 17: return QCPColorGradient::cbGreys;
    case 18: return QCPColorGradient::gpGrayscale;
    case 19: return QCPColorGradient::gpNight;
    case 20: return QCPColorGradient::gpCandy;
    case 21: return QCPColorGradient::gpIon;
    case 22: return QCPColorGradient::gpThermal;
    case 23: return QCPColorGradient::gpPolar;
    case 24: return QCPColorGradient::gpSpectrum;
    case 25: return QCPColorGradient::gpJet;
    case 26: return QCPColorGradient::gpHues;
    case 27: return QCPColorGradient::gpHot;
    case 28: return QCPColorGradient::gpCold;
    case 29: return QCPColorGradient::cbBrBG;
    case 30: return QCPColorGradient::cbPiYG;
    case 31: return QCPColorGradient::cbPRGn;
    case 32: return QCPColorGradient::cbPuOr;
    case 33: return QCPColorGradient::cbRdBu;
    case 34: return QCPColorGradient::cbRdGy;
    case 35: return QCPColorGradient::cbRdYlBu;
    case 36: return QCPColorGradient::cbRdYlGn;
    case 37: return QCPColorGradient::cbSpectral;
    case 38: return QCPColorGradient::vSpectral;
    default: return QCPColorGradient::gpCold;
    }
}

///
/// \brief VespucciDataset::GetClusterGradient
/// Cluster gradients are slightly different from the continuous gradients. This
/// selects the right gradient based on the number of clusters.
/// \param clusters Number of clusters
/// \return Proper color gradient for number of clusters
///
QCPColorGradient VespucciDataset::GetClusterGradient(int clusters)
{
    switch (clusters)
    {
    case 2: return QCPColorGradient::cbCluster2;
    case 3: return QCPColorGradient::cbCluster3;
    case 4: return QCPColorGradient::cbCluster4;
    case 5: return QCPColorGradient::cbCluster5;
    case 6: return QCPColorGradient::cbCluster6;
    case 7: return QCPColorGradient::cbCluster7;
    case 8: return QCPColorGradient::cbCluster8;
    case 9: return QCPColorGradient::cbCluster9;
    default: return QCPColorGradient::cbCluster9;
    }
}

///
/// \brief VespucciDataset::ConstructorCancelled
/// Specifies whether or not the constructor has been canceled. The constructor
/// asks this and cleans everything up in case it is canceled.
/// \return
///
bool VespucciDataset::ConstructorCancelled()
{
    return constructor_canceled_;
}

///
/// \brief VespucciDataset::AverageSpectrum
/// Finds the average of the spectrum. This can be saved by the user.
/// Probably not all that useful, except for determining a spectrum to use as a
/// background spectrum for other maps.
/// \param stats Whether or not to include standard deviations on the second row.
/// \return The average spectrum
///
mat VespucciDataset::AverageSpectrum(bool stats)
{
    mat spec_mean = mean(spectra_, 0);
    rowvec spec_stddev;
    spec_mean = mean(spectra_, 0);
    //insert stddevs on next line if requested
    if (stats){
        spec_stddev = stddev(spectra_, 0);
        spec_mean.insert_rows(1, spec_stddev);
    }
    return spec_mean;
}



///
/// \brief VespucciDataset::x_axis_description
/// The x_axis_description is printed on the spectrum viewer.
/// \return Spectral abcissa description.
///
const QString VespucciDataset::x_axis_description()
{
    return x_axis_description_;
}

///
/// \brief VespucciDataset::SetXDescription
/// Sets the value of the spectral abcissa description.
/// \param description
///
void VespucciDataset::SetXDescription(QString description)
{
    x_axis_description_ = description;
}

///
/// \brief VespucciDataset::SetYDescription
/// \param description
/// Sets the value of the spectral ordinate axis description
void VespucciDataset::SetYDescription(QString description)
{
    y_axis_description_ = description;
}

///
/// \brief VespucciDataset::y_axis_description
/// \return The spectral ordinate axis description.
///
const QString VespucciDataset::y_axis_description()
{
    return y_axis_description_;
}

///
/// \brief VespucciDataset::principal_components_calculated
/// Accessor for principal_components_calculated_. The PCA dialog requests this
/// to make sure that the same PCA is not calculated twice.
/// \return Whether or not PCA has been calculated.
///
bool VespucciDataset::principal_components_calculated()
{
    return principal_components_calculated_;
}

///
/// \brief VespucciDataset::vertex_components_calculated
/// Accessor for vertex_components_calculated_. The VCA dialog requests this to
/// make sure that the same VCA is not calculated twice.
/// \return Whether or not VCA has been computed.
///
bool VespucciDataset::vertex_components_calculated()
{
    return vertex_components_calculated_;
}

///
/// \brief VespucciDataset::partial_least_squares_calculated
/// Accessor for partial_least_squares_calculated. The PLS dialog requests this
/// to make sure that the same PLS is not calculated twice.
/// \return Whether or not PLS has been computed.
///
bool VespucciDataset::partial_least_squares_calculated()
{
    return partial_least_squares_calculated_;
}

///
/// \brief VespucciDataset::k_means_calculated
/// Accessor for k_means_calculated_. Used for filling dataviewer.
/// \return Whether or not k means have been calculated.
///
bool VespucciDataset::k_means_calculated()
{
    return k_means_calculated_;
}

///
/// \brief VespucciDataset::principal_components_data
/// Accessor for principal_components_data_
/// \return Pointer to the class that handles the output of arma::princomp
///
PrincipalComponentsData* VespucciDataset::principal_components_data()
{
    return principal_components_data_;
}

///
/// \brief VespucciDataset::vertex_components_data
/// Accessor for vertex_components_data_
/// \return
///
VCAData* VespucciDataset::vertex_components_data()
{
    return vertex_components_data_;
}

///
/// \brief VespucciDataset::partial_least_squares_data
/// Accessor for partial_least_squares_data_;
/// \return
///
PLSData* VespucciDataset::partial_least_squares_data()
{
    return partial_least_squares_data_;
}

mat *VespucciDataset::k_means_data()
{
    return &k_means_data_;
}

///
/// \brief VespucciDataset::spectra_ptr
/// \return
///
mat* VespucciDataset::spectra_ptr()
{
    return &spectra_;
}

///
/// \brief VespucciDataset::wavelength_ptr
/// \return
///
mat* VespucciDataset::wavelength_ptr()
{
    return &wavelength_;
}

///
/// \brief VespucciDataset::x_ptr
/// \return
///
mat* VespucciDataset::x_ptr()
{
    return &x_;
}

///
/// \brief VespucciDataset::y_ptr
/// \return
///
mat* VespucciDataset::y_ptr()
{
    return &y_;
}

///
/// \brief VespucciDataset::non_spatial
/// \return True if map has empty x_ and y_
///
bool VespucciDataset::non_spatial()
{
    return non_spatial_;
}

///
/// \brief VespucciDataset::last_operation
/// \return Description of last pre-processing operation performed
///
QString VespucciDataset::last_operation()
{
    return last_operation_;
}
