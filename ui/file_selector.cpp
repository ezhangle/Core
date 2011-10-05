#include "file_selector.h"

UIFileSelector::UIFileSelector(const std::string& title, const std::string& dir, UIFileSelectorAction* action)
    : action(action), UIGroup(title, true, true) {

    minimizable = false;

    listing   = new UIScrollLayout(vec2(420.0f, 100.0f));

    std::string initial_dir = dir;

    // remove trailing slash
    if(!initial_dir.size() > 2 && initial_dir[1] != ':' && (initial_dir[initial_dir.size()-1] == '/' || initial_dir[initial_dir.size()-1] == '\\')) {
        initial_dir.resize(dir.size() - 1);
    }

    dir_path  = new UIDirInputLabel(this, initial_dir);
    file_path = new UIFileInputLabel(this, "");

    filter_select = new UISelect();

    layout->addElement(new UILabelledElement("Path",  dir_path,  120.0f));
    layout->addElement(listing);
    layout->addElement(new UILabelledElement("Name",  file_path, 120.0f));
    layout->addElement(new UILabelledElement("Filter", filter_select, 120.0f));

    addFilter("All Files (*.*)", "");

    current_filter = filter_select->getSelectedOption();

    updateListing();
}

bool _listing_sort (const boost::filesystem::path& a,const boost::filesystem::path& b) {
    bool dir_a = is_directory(a);
    bool dir_b = is_directory(b);

    if(dir_a != dir_b) return dir_b < dir_a;

    return boost::ilexicographical_compare(a.filename().string(), b.filename().string());
}

void UIFileSelector::addFilter(const std::string& name, const std::string& extension) {
    filter_select->addOption(name, extension);
}

bool UIFileSelector::changeDir(const boost::filesystem::path& dir) {

    if(!is_directory(dir)) return false;

    std::string path_string = dir.string();

#ifdef _WIN32
    if(path_string.size() == 2 && path_string[1] == ':') {
        path_string += "\\";
    }
#endif

    previous_dir = dir_path->text;

    dir_path->setText(path_string);

    updateListing();

    return true;
}

void UIFileSelector::update(float dt) {

    UIOptionLabel* selected_filter = filter_select->getSelectedOption();

    if(current_filter != selected_filter) {
        current_filter = selected_filter;
        updateListing();
    }

    UIGroup::update(dt);
}

void UIFileSelector::toggle() {
    if(hidden) open();
    else close();
}

void UIFileSelector::open() {
    hidden=false;
    updateListing();
    if(ui!=0) ui->selectElement(file_path);
}

void UIFileSelector::close() {
    filter_select->open = false;
    hidden=true;
}

void UIFileSelector::selectFile(const boost::filesystem::path& filename) {

    if(exists(filename)) {
        selected_path = filename;
    } else {
        selected_path = boost::filesystem::path(dir_path->text);
        selected_path /= filename;
    }

    fprintf(stderr, "selectedFile = %s\n", selected_path.string().c_str());
}

void UIFileSelector::selectPath(const boost::filesystem::path& path) {
    this->selected_path = path;

    if(!is_directory(selected_path)) {
        file_path->setText(selected_path.filename().string());
    } else {
        file_path->setText("");
    }
}

void UIFileSelector::confirm() {
    action->perform(selected_path);
}

void UIFileSelector::updateListing() {

    if(dir_path->text.empty()) return;

    boost::filesystem::path p(dir_path->text);

    if(!is_directory(p)) return;

    std::vector<boost::filesystem::path> dir_listing;

    try {
        copy(boost::filesystem::directory_iterator(p), boost::filesystem::directory_iterator(), back_inserter(dir_listing));
        std::sort(dir_listing.begin(), dir_listing.end(), _listing_sort);
    } catch(const boost::filesystem::filesystem_error& exception) {

        //switch to previous directory if there is one
        if(!previous_dir.empty() && is_directory(boost::filesystem::path(previous_dir))) {
            dir_path->setText(previous_dir);
            previous_dir = "";
        }

        return;
    }

    listing->clear();

    boost::filesystem::path parent_path = p.parent_path();

    if(is_directory(parent_path) && !(parent_path.string().size() == 2 && parent_path.string()[1] == ':')) {
        listing->addElement(new UIFileSelectorLabel(this, "..", parent_path));
    }

    foreach(boost::filesystem::path l, dir_listing) {

        std::string filename(l.filename().string());

        if(filename.empty()) continue;

#ifdef _WIN32
        DWORD win32_attr = GetFileAttributes(l.string().c_str());
        if (win32_attr & FILE_ATTRIBUTE_HIDDEN || win32_attr & FILE_ATTRIBUTE_SYSTEM) continue;
#else
        if(filename[0] == '.') continue;
#endif

        if(current_filter != 0 && !current_filter->value.empty() && !is_directory(l)) {
            size_t at = filename.rfind(current_filter->value);

            if(at == std::string::npos || at != (filename.size() - current_filter->value.size()))
                continue;
        }

        listing->addElement(new UIFileSelectorLabel(this, l));
    }

    listing->update(0.1f);

    listing->horizontal_scrollbar->reset();
    listing->vertical_scrollbar->reset();

    listing->vertical_scrollbar->bar_step = 1.0f / listing->getElementCount();
}

std::string UIFileSelector::autocomplete(const std::string& input, bool dirs_only) {

    //find all files/dirs prefixed by input
    //find minimum set of common characters to expand input by

    int count = listing->getElementCount();

    std::list<UIFileSelectorLabel*> matches;

    //normalize
    boost::filesystem::path input_path(input);

    boost::filesystem::path parent_path = input_path.parent_path();

    if(!is_directory(parent_path)) return input;

    std::vector<boost::filesystem::path> dir_listing;

    try {
        copy(boost::filesystem::directory_iterator(parent_path), boost::filesystem::directory_iterator(), back_inserter(dir_listing));
    } catch(const boost::filesystem::filesystem_error& exception) {
        return input;
    }

    std::string input_path_string = input_path.string();

    size_t input_path_size = input_path_string.size();

    std::string match;

    foreach(boost::filesystem::path l, dir_listing) {

        if(dirs_only && !is_directory(l)) continue;

        std::string path_string = l.string();
        std::string filename(l.filename().string());

        if(filename.empty()) continue;
#ifdef _WIN32
        DWORD win32_attr = GetFileAttributes(l.string().c_str());
        if (win32_attr & FILE_ATTRIBUTE_HIDDEN || win32_attr & FILE_ATTRIBUTE_SYSTEM) continue;
#else
        if(filename[0] == '.') continue;
#endif

        //check if input is a prefix of this string
        if(path_string.find(input_path_string) == 0) {

            if(match.empty()) match = path_string;
            else {
                //find minimum subset of current match longer than input that
                //is also a prefix of this string
                while(!match.empty() && match.size() > input_path_size) {
                    if(path_string.find(match) == 0) {
                        break;
                    }
                    match.resize(match.size()-1);
                }
            }
        }
    }

    if(match.size() > input.size()) return match;

    return input;
}

//UIFileSelectorLabel

UIFileSelectorLabel::UIFileSelectorLabel(UIFileSelector* selector, const std::string& label, const boost::filesystem::path& path)
    : selector(selector), path(path), UILabel(label, false, 420.0f) {
    directory = is_directory(path);
}

UIFileSelectorLabel::UIFileSelectorLabel(UIFileSelector* selector, const boost::filesystem::path& path)
    : selector(selector), path(path), UILabel(path.filename().string(), false, 420.0f) {
    directory = is_directory(path);
}

void UIFileSelectorLabel::updateContent() {
    font_colour = selected  ? vec3(1.0f) :
                  directory ? vec3(0.0f, 1.0f, 1.0f) : vec3(0.0f, 1.0f, 0.0f);
    bgcolour = selected ? vec4(1.0f, 1.0f, 1.0f, 0.15f) : vec4(0.0f);
}

void UIFileSelectorLabel::click(const vec2& pos) {
    if(!directory) selector->selectPath(path);
}

void UIFileSelectorLabel::doubleClick(const vec2& pos) {
    submit();
}

bool UIFileSelectorLabel::submit() {

    if(directory) {
        ui->deselect();
        selector->changeDir(path);
    } else {
        selector->selectPath(path);
        selector->confirm();
    }

    return true;
}

//UIDirInputLabel
UIDirInputLabel::UIDirInputLabel(UIFileSelector* selector, const std::string& dirname)
    : selector(selector), UILabel(dirname, true, 300.0f) {
}


bool UIDirInputLabel::submit() {
    selector->changeDir(text);
    return true;
}

void UIDirInputLabel::tab() {
    setText(selector->autocomplete(text, true));
}

//UIFileInputLabel
UIFileInputLabel::UIFileInputLabel(UIFileSelector* selector, const std::string& filename)
    : selector(selector), UILabel(filename, true,  300.0f) {
}

void UIFileInputLabel::tab() {
    setText(selector->autocomplete(text));
}

bool UIFileInputLabel::submit() {

    boost::filesystem::path filepath(text);

    if(is_directory(filepath)) {
        selector->changeDir(filepath);
        return true;
    }

    selector->selectFile(filepath);
    selector->confirm();
    return true;
}